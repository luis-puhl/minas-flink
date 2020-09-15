package br.ufscar.dc.ppgcc.gsdr.minas

import br.ufscar.dc.ppgcc.gsdr.minas.datasets.kdd._
import br.ufscar.dc.ppgcc.gsdr.minas.kmeans._
import grizzled.slf4j.Logger
import org.apache.flink.api.scala.ExecutionEnvironment
import org.apache.flink.core.fs.FileSystem
import org.apache.flink.streaming.api.scala._

object MinasFlinkOffline extends App {
  val LOG = Logger(getClass)
  val jobName = this.getClass.getName
  LOG.info(s"jobName = $jobName")
  val setEnv = ExecutionEnvironment.getExecutionEnvironment

  val inPathIni = "./tmpfs/KDDTe5Classes_fold1_ini.csv"
  val inPathOnl = "./tmpfs/KDDTe5Classes_fold1_onl.csv"
  val outFilePath = "./tmpfs/out"
  val iterations = 10
  val k = 100
  val varianceThreshold = 1.0E-5

  setEnv.readTextFile(inPathIni)
    .map(line => KddCassalesEntryFactory.fromStringLine(line))
    .map(entry => (entry.label, entry.value)
    .toVector
  bufferedSource.close

  def afterConsumedHook(arg: (Option[String], Point, Cluster, Double)): Unit = {
    LOG.info(s"afterConsumedHook $arg")
  }

  val minas: Minas = Minas(config, Point.EuclideanSqrDistance)
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
