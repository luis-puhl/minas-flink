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

  //---------
  val training$2: DataStream[(String, Point)] = streamEnv.readTextFile(inPathIni)
    .map(line => KddCassalesEntryFactory.fromStringLine(line))
    .keyBy(p => 0)
    .mapWithState[(String, Point), Long]((entry: KddCassalesEntry, counterState: Option[Long]) => {
      val counter = counterState match {
        case Some(count) => count
        case None => 0L
      }
      ((entry.label, Point(counter, entry.value)), Some(counter + 1L))
    })
  training$2.writeAsText(outFilePath + "/stream-ini.txt", FileSystem.WriteMode.OVERWRITE)

  val clusters$ = training$2
    // .assignAscendingTimestamps(p => p._2.id)
    .keyBy(p => p._1)
    // .keyBy(p => p._2.id % training$2.parallelism)
    // .keyBy(p => "0")
    // .process(new CountOrTimeoutWindow[String, (String, Point)](1000, 100))
    .timeWindow(Time.milliseconds(100)).aggregate(new ToVectAggregateFunction).flatMap(map => map.toVector)
    .map(agg => {
      val label = agg._1
      val points = agg._2
      LOG.info(s"Taking in ${points.size} ${label}")
      (label, points.head.id, points)
    })
  clusters$
    .flatMap(p => p._3)
    .writeAsText(outFilePath + "/CountOrTimeoutWindow", FileSystem.WriteMode.OVERWRITE)
  val clustersFinal = clusters$
    .keyBy(p => p._1 + p._2)
    .map(labelPoints => {
      val label = labelPoints._1
      val points = labelPoints._3.sortBy(p => p.fromOrigin)
      // LOG.info(s"Taking in ${points.size} from $label.")
      val c0 = Vector(points.head, points.last).map(p => Cluster(p.id, p, 0.0))
      val distancesMap = Kmeans.groupByClosest(points, c0)
      val clusters = Kmeans.updateClustersVariance(distancesMap)
      val results = Kmeans.kmeans(label, points, clusters)
      (label, results, points)
    })
  clustersFinal.writeAsText(outFilePath + "/basic-kmeans.txt", FileSystem.WriteMode.OVERWRITE)
  clustersFinal
    .flatMap(p => p._3.map(d => (d, p._1)))
    .keyBy(p => p._1.id % clustersFinal.parallelism)
    .writeAsText(outFilePath + "/consumed-points.txt", FileSystem.WriteMode.OVERWRITE)
  //
  streamEnv.execute("base centroids stream")
}
