package br.ufscar.dc.ppgcc.gsdr.minas

import br.ufscar.dc.ppgcc.gsdr.minas.datasets.kdd._
import br.ufscar.dc.ppgcc.gsdr.minas.kmeans._
import br.ufscar.dc.ppgcc.gsdr.minas.kmeans.KMeansVector
import grizzled.slf4j.Logger
import org.apache.flink.api.scala._
import org.apache.flink.core.fs.FileSystem
import org.apache.flink.streaming.api.scala.{DataStream, StreamExecutionEnvironment}

object MinasKddCassales extends App {
  val LOG = Logger(getClass)
  val streamEnv = StreamExecutionEnvironment.getExecutionEnvironment
  val setEnv = ExecutionEnvironment.getExecutionEnvironment

  val inPathIni = "./tmpfs/KDDTe5Classes_fold1_ini.csv"
  val inPathOnl = "./tmpfs/KDDTe5Classes_fold1_onl.csv"
  val outFilePath = "./tmpfs/out"
  val iterations = 10
  val k = 100
  val varianceThreshold = 1.0E-5

  val trainingSet: DataSet[KddCassalesEntry] = setEnv.readCsvFile[KddCassalesEntry](inPathIni)
  trainingSet.writeAsText(outFilePath + "/kdd-ini", FileSystem.WriteMode.OVERWRITE)

  val pointsTrainingSet: DataSet[(String, Point)] =
    org.apache.flink.api.scala.utils.DataSetUtils(trainingSet).zipWithUniqueId.rebalance()
    .map(entry => (entry._2.label, Point(entry._1, entry._2.value)))

  val clusters = pointsTrainingSet
    .groupBy(x => x._1)
    .reduceGroup((all: Iterator[(String, Point)]) => {
      val allVector: Vector[(String, Point)] = all.toVector
      val label: String = allVector.head._1
      val points: Seq[Point] = allVector.map(k => k._2)
      val dataPoints = points.size
      LOG.info(s"Label '$label' contains $dataPoints data points.")
      val clusters = KMeansVector.kmeans(label, k, points, iterations, varianceThreshold)
      (label, clusters)
    })
  clusters.writeAsText(outFilePath + "/clusters", FileSystem.WriteMode.OVERWRITE)

  // setEnv.execute("base centroids")

  //
  val ini$ = streamEnv.readTextFile(inPathIni).map(line => KddCassalesEntryFactory.fromStringLine(line))
  ini$.writeAsCsv(outFilePath + "/stream-ini.csv", FileSystem.WriteMode.OVERWRITE)
  ini$.writeAsText(outFilePath + "/stream-ini.txt", FileSystem.WriteMode.OVERWRITE)
  streamEnv.execute("base centroids stream")
}
