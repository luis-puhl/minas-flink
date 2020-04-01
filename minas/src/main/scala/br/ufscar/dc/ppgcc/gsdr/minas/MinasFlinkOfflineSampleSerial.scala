package br.ufscar.dc.ppgcc.gsdr.minas

import java.io.{File, FileWriter}
import java.time.LocalDateTime
import java.time.format.DateTimeFormatter

import br.ufscar.dc.ppgcc.gsdr.minas.kmeans._
import grizzled.slf4j.Logger
import org.apache.flink.api.common.typeinfo.{TypeInformation, _}
import org.apache.flink.api.scala._

import scala.collection.{AbstractIterator, Iterator}

object MinasFlinkOfflineSampleSerial {
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
        val label: String = lineSplit.last
        (label, Point(index, doubles))
      })
    //
    LOG.info(s"training.parallelism = ${training.parallelism}")
    training.writeAsText(s"$outDir/initial")
    //
    val serialKMeans = training
      .groupBy(_._1) // group by label
      .reduceGroup(datapoints => {
        // [ 1k ] -> { azul: 500, vermelho, 500 } -> { azul: [k], vermelho: [k] }
        val dataSeq = datapoints.toSeq
        // datapoints => somente azuis
        val label = dataSeq.head._1
        val points = dataSeq.map(_._2) // extrair os point do iterador
        //
        val initial: Seq[Cluster] = Kmeans.kmeansInitialRandom(label, k, points)
        // ponto e o centro do cluster
        val clusters = Kmeans.kmeans(label, points, initial)
        // minimizar a variancia total, movendo os centros para o ponto medio dentro do cluster
        (label, clusters)
      })
      .flatMap(i => i._2)
    serialKMeans.writeAsText(s"$outDir/serialKmeans")

    setEnv.execute(jobName)
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
