package br.ufscar.dc.ppgcc.gsdr.minas

import java.io.{File, FileWriter}
import java.time.LocalDateTime
import java.time.format.DateTimeFormatter

import grizzled.slf4j.Logger
import br.ufscar.dc.ppgcc.gsdr.minas.kmeans._
import org.apache.flink.api.common.functions.{MapFunction, RichMapFunction}
import org.apache.flink.api.common.typeinfo.{TypeInformation, _}
import org.apache.flink.api.java.functions.FunctionAnnotation.ForwardedFields
import org.apache.flink.api.scala._
import org.apache.flink.configuration.Configuration

import scala.collection.{AbstractIterator, Iterator}
import scala.collection.JavaConverters._

object MinasFlinkOffline {
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
    serialKMeans(outDir, k, training)

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

    val fixedLoop = (0 to iterations).foldLeft(initialClusters)((prev, i) => {
      val next = iterationKMeansFlink(prev, training)
      next.writeAsText(s"$outDir/parallelKMeans-fixedLoop-$i")
      next
    })

//    val fromFlinkExamples = initialClusters.iterate(iterations) { currentCentroids =>
//      val newCentroids = training
//        .withForwardedFields("*->_2")
//        .map(new RichMapFunction[Point, (Long, Point)] {
//          private var centroids: Traversable[Cluster] = _
//
//          /** Reads the centroid values from a broadcast variable into a collection. */
//          override def open(parameters: Configuration): Unit = {
//            centroids = getRuntimeContext.getBroadcastVariable[Cluster]("centroids").asScala
//          }
//
//          override def map(p: Point): (Long, Point) = {
//            var minDistance: Double = Double.MaxValue
//            var closestCentroidId: Long = -1
//            for (centroid <- centroids) {
//              val distance = p.euclideanDistance(centroid.center)
//              if (distance < minDistance) {
//                minDistance = distance
//                closestCentroidId = centroid.id
//              }
//            }
//            (closestCentroidId, p)
//          }
//        })
//        .withBroadcastSet(currentCentroids, "centroids")
//        .map { x => (x._1, x._2, 1L) }.withForwardedFields("_1; _2")
//        .groupBy(0)
//        .reduce { (p1, p2) => (p1._1, p1._2.add(p2._2), p1._3 + p2._3) }.withForwardedFields("_1")
//        .map { x => new Centroid(x._1, x._2.div(x._3)) }.withForwardedFields("_1->id")
//      newCentroids
//    }

    initialClusters
      .iterateWithTermination(iterations)(previous => {
        val previousVar = previous.groupBy(c => c._1)
          .reduceGroup(g => g.foldLeft(("", 0.0))((state, next) => (next._1, state._2 + next._2.variance)))
        val next = iterationKMeansFlink(previous, training)
        val term = next.groupBy(c => c._1)
          .reduceGroup(g => g.foldLeft(("", 0.0))((state, next) => (next._1, state._2 + next._2.variance)))
          .join(previousVar).where(0).equalTo(0)
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

  private def serialKMeans(outDir: String, k: Int, training: DataSet[(String, Point)]) = {
    training
      .groupBy(_._1)
      .reduceGroup(datapoints => {
        val dataSeq = datapoints.toSeq
        val label = dataSeq.head._1
        val points = dataSeq.map(_._2)
        val initial = Kmeans.kmeansInitialRandom(label, k, points)
        val clusters = Kmeans.kmeans(label, points, initial)
        (label, clusters)
      })
      .flatMap(i => i._2)
      .writeAsText(s"$outDir/serialKmeans")
  }

  private def indexInputFile(inPathIni: String, inPathIndexed: String) = {
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
