package br.ufscar.dc.ppgcc.gsdr.minas

import br.ufscar.dc.ppgcc.gsdr.minas.datasets.kdd._
import br.ufscar.dc.ppgcc.gsdr.minas.helpers.{CountOrTimeoutWindow, ToVectAggregateFunction}
import br.ufscar.dc.ppgcc.gsdr.minas.kmeans._
import br.ufscar.dc.ppgcc.gsdr.minas.kmeans.Kmeans
import grizzled.slf4j.Logger
import org.apache.flink.api.scala.{DataSet, ExecutionEnvironment, extensions}
import org.apache.flink.streaming.api.scala._
import org.apache.flink.streaming.api.windowing.time.Time
import org.apache.flink.core.fs.FileSystem

object MinasKddApp extends App {
  val LOG = Logger(getClass)
  val jobName = this.getClass.getName
  LOG.info(s"jobName = $jobName")
  val streamEnv = StreamExecutionEnvironment.getExecutionEnvironment
  val setEnv = ExecutionEnvironment.getExecutionEnvironment

  val inPathIni = "./tmpfs/KDDTe5Classes_fold1_ini.csv"
  val inPathOnl = "./tmpfs/KDDTe5Classes_fold1_onl.csv"
  val outFilePath = "./tmpfs/out"
  val iterations = 10
  val k = 100
  val varianceThreshold = 1.0E-5

  val config: Map[String, Int] = Map(
    "k" -> k,
    "noveltyDetectionThreshold" -> 1000,
    "representativeThreshold" -> 10,
    "noveltyIndex" -> 0
  )

  val bufferedSource = io.Source.fromFile(inPathIni)
  val stream: Vector[(String, Point)] = bufferedSource.getLines
    .map(line => KddCassalesEntryFactory.fromStringLine(line))
    .zipWithIndex
    .map(entry => (entry._1.label, Point(entry._2, entry._1.value)))
    .toVector
  bufferedSource.close

  def afterConsumedHook(arg: (Option[String], Point, Cluster, Double)): Unit = {
    LOG.info(s"afterConsumedHook $arg")
  }

  val minas: Minas = Minas(config, afterConsumedHook, Point.EuclideanSqrDistance)
  val model: MinasModel = minas.offline(stream)

  val testStream: DataStream[(String, Point)] = streamEnv.readTextFile(inPathOnl)
    .map(line => KddCassalesEntryFactory.fromStringLine(line))
    .keyBy(p => 0)
    .mapWithState[(String, Point), Long]((entry: KddCassalesEntry, counterState: Option[Long]) => {
      val counter = counterState match {
        case Some(count) => count
        case None => 0L
      }
      if (!entry.isNormalized) LOG.info("out of normal form")
      ((entry.label, Point(counter, entry.value)), Some(counter + 1L))
    })
  testStream.writeAsText(outFilePath + "/testStream", FileSystem.WriteMode.OVERWRITE)

  val result = minas.online(model, testStream.map(_._2))
  result.writeAsText(outFilePath + "/result", FileSystem.WriteMode.OVERWRITE)

  streamEnv.execute(jobName)
}
