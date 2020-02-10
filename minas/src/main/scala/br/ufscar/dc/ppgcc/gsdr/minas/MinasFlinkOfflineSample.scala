package br.ufscar.dc.ppgcc.gsdr.minas

import java.io.{File, FileWriter}
import java.time.LocalDateTime
import java.time.format.DateTimeFormatter

import br.ufscar.dc.ppgcc.gsdr.minas.kmeans._
import grizzled.slf4j.Logger
import org.apache.flink.api.common.typeinfo.{TypeInformation, _}
import org.apache.flink.api.scala._

import scala.collection.{AbstractIterator, Iterator}

object MinasFlinkOfflineSample {
  val LOG = Logger(getClass)
  def main(args: Array[String]): Unit = {
    val jobName = this.getClass.getName
    val dateString = LocalDateTime.now.format(DateTimeFormatter.ISO_DATE_TIME).replaceAll(":", "-")
    LOG.info(s"jobName = $jobName")
    val setEnv = ExecutionEnvironment.getExecutionEnvironment

    val inPathIni = "/home/puhl/project/minas-flink/ref/KDDTe5Classes_fold1_ini.csv"
    val inPathIndexed = s"$inPathIni.indexed"
    val outDir = s"./out/$jobName/$dateString/"
    val dir = new File(outDir)
    if (!dir.exists) {
      if (!dir.mkdirs) throw new RuntimeException(s"Output directory '$outDir'could not be created.")
    }
    val iterations = 10
    val k = 100
    val varianceThreshold = 1.0E-5

    indexInputFile(inPathIni, inPathIndexed)

    val training: DataSet[(String, Point)] = setEnv.readTextFile(inPathIndexed)
      .map(line => {
        // espera que a linha seja "int,double[],label"
        val lineSplit = line.split(",")
        val index = lineSplit.head.toLong
        val doubles = lineSplit.slice(1, lineSplit.length - 1).map(_.toDouble)
        val label = lineSplit.last
        (label, Point(index, doubles))
      })
    //
    LOG.info(s"training.parallelism = ${training.parallelism}")
    training.writeAsText(s"$outDir/initial")
    //
    val parallelKMeansInitial: DataSet[(String, Seq[Cluster], Seq[Point])] = training
      .groupBy(_._1)
      .reduceGroup(datapoints => {
        val dataSeq = datapoints.toSeq
        val label = dataSeq.head._1
        val points = dataSeq.map(_._2)
        val initial = points.take(k).map(p => Cluster(p.id, p, 0.0, label))
        (label, initial, points)
      })
    val initialClusters: DataSet[(String, Cluster)] = parallelKMeansInitial.flatMap(a => a._2.map(c => (c.label, c)))
    initialClusters.writeAsText(s"$outDir/parallelKMeans-initialClusters")
    //

    def extractVariance(dataset: DataSet[(String, Cluster)]): DataSet[(String, Double)] =
      dataset.groupBy(c => c._1)
        .reduceGroup(g => g.foldLeft(("", 0.0))((state, next) => (next._1, state._2 + next._2.variance)))

    initialClusters
      .iterateWithTermination(iterations)(previous => {
        val previousVariance: DataSet[(String, Double)] = extractVariance(previous)
        val next: DataSet[(String, Cluster)] = iterationKMeansFlink(previous, training)
        val term = extractVariance(next)
          .join(previousVariance).where(0).equalTo(0)
          .map(v => (v._1._1, v._1._2 / v._2._2))
          .filter(v => v._2 < varianceThreshold)
        (next, term)
      })
      .writeAsText(s"$outDir/parallelKMeans-finalClusters")

    setEnv.execute(jobName)
  }

  def iterationKMeansFlink(clusters: DataSet[(String, Cluster)], points: DataSet[(String, Point)]): DataSet[(String, Cluster)] = {
    clusters
      .crossWithHuge(points)(
        (c, p) => if (p._1 != c._1) Seq.empty else Seq((p._1, p._2.id, c._2.id, p._2, c._2, p._2.distance(c._2.center)))
      )
      .flatMap(i => i)
      .groupBy(0, 1).min(5)
      .groupBy(1, 2).reduceGroup(g => {
      val sq = g.toSeq
      val h = sq.head
      (h._1, h._3, h._5, sq.map(i => (i._4, i._6)))
    })
      .map(group => {
        val label = group._1
        val c = group._3
        val nextCl = Cluster(c.id, Point.zero(c.center.dimension), 0.0, c.label)
        val next = group._4.foldLeft(nextCl)((state: Cluster, next: (Point, Double)) => {
          val p = next._1
          val d = next._2
          state.consumeWithDistance(p, d, group._4.size)
        })
        (label, next)
      })
  }
  private def indexInputFile(inPathIni: String, inPathIndexed: String): Unit = {
    // adicionando uid as linhas
    val indexedFile = new File(inPathIndexed)
    if (!indexedFile.exists) {
      LOG.info("indexing input file.")
      val bufferedSource = io.Source.fromFile(inPathIni)
      val contents = bufferedSource.getLines.zipWithLongIndex
      indexedFile.createNewFile()
      val w = new FileWriter(indexedFile)
      contents.foreach(i => {
        val text = f"${i._2}%d,${i._1}%s%n" // String.format("%1$d,%2$s%n", i._2, i._1)
        w.write(text)
      })
      w.close()
      bufferedSource.close
    }
  }

  implicit class RichIterator[A](val self: Iterator[A]) extends AnyVal {
    def zipWithLongIndex: Iterator[(A, Long)] = new AbstractIterator[(A, Long)] {
      var idx: Long = 0L

      def hasNext: Boolean = self.hasNext

      def next: (A, Long) = {
        val ret = (self.next(), idx)
        idx += 1
        ret
      }
    }
  }

  implicit class RichSet[T](val value: DataSet[T]) extends AnyVal {
    def parallelism: Int = value.getParallelism
  }

}
