package br.ufscar.dc.ppgcc.gsdr.minas

import br.ufscar.dc.ppgcc.gsdr.minas.datasets.kdd._
import br.ufscar.dc.ppgcc.gsdr.minas.kmeans._
import br.ufscar.dc.ppgcc.gsdr.minas.kmeans.KMeansVector
import grizzled.slf4j.Logger
import org.apache.flink.api.common.state.{ValueState, ValueStateDescriptor}
import org.apache.flink.api.scala.{DataSet, ExecutionEnvironment, extensions}
import org.apache.flink.streaming.api.scala._
import org.apache.flink.streaming.api.windowing.time.Time
import org.apache.flink.core.fs.FileSystem
import org.apache.flink.streaming.api.functions.KeyedProcessFunction
import org.apache.flink.streaming.api.windowing.assigners.WindowAssigner
import org.apache.flink.util.Collector

import scala.collection.immutable.HashMap

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
      LOG.info(s"Label '$label' contains ${points.size} data points.")
      val kmeansClusters = KMeansVector.kmeans(label, k, points, iterations, varianceThreshold)
      (label, kmeansClusters)
    })
  clusters.writeAsText(outFilePath + "/clusters", FileSystem.WriteMode.OVERWRITE)
  setEnv.execute("base centroids")

  //
  val optionTrainingVector: Seq[Option[KddCassalesEntry]] = trainingSet
    .map(p => Option(p))
    .collect()
    .++(Seq(Option.empty[KddCassalesEntry]))
  // val training$: DataStream[KddCassalesEntry] = streamEnv.readTextFile(inPathIni)
  //   .map(line => KddCassalesEntryFactory.fromStringLine(line))
  val training$: DataStream[Option[KddCassalesEntry]] = streamEnv.fromCollection(optionTrainingVector)
  // training$.writeAsCsv(outFilePath + "/stream-ini.csv", FileSystem.WriteMode.OVERWRITE)
  // training$.writeAsText(outFilePath + "/stream-ini.txt", FileSystem.WriteMode.OVERWRITE)

  // zipWithUniqueId
  val pointsTraining$: DataStream[Option[(String, Point)]] = training$
    .keyBy(p => 0)
    .mapWithState((optionEntry: Option[KddCassalesEntry], counterState: Option[Long]) => {
      var counter = counterState match {
        case Some(count) => count
        case None => 0L
      }
      val point: Option[(String, Point)] = optionEntry match {
        case Some(entry) => {
          counter += 1L
          Option[(String, Point)](entry.label, Point(counter, entry.value))
        }
        case None => None
      }
      (point, Some(counter))
    })
    .setParallelism(1)

  class Transformer extends KeyedProcessFunction[String, Option[(String, Vector[Point])], (String, Vector[Point])]() {
    type StateType = Map[String, Vector[Point]]
    lazy val state: ValueState[StateType] = getRuntimeContext
      .getState(new ValueStateDescriptor[StateType]("myState", classOf[StateType]))
    override def processElement(
       value: Option[(String, Vector[Point])],
       ctx: KeyedProcessFunction[String, Option[(String, Vector[Point])], (String, Vector[Point])]#Context,
       out: Collector[(String, Vector[Point])]
    ): Unit = {
      // initialize or retrieve/update the state
      val current: StateType = state.value match {
        case null => HashMap[String, Vector[Point]]()
        case other => other
      }
      LOG.info(s"Received ${current.size} ${current.mapValues(v => v.size)}")
      val updated: Map[String, Vector[Point]] = value match {
        case Some((label: String, values: Vector[Point])) => {
          val stored: Seq[Point] = current.getOrElse(label, Seq[Point]())
          current + (label ->  values.++(stored))
        }
        case None => {
          // side effect
          LOG.info(s"Flush ${current.size} labels")
          current.foreach(p => {
            LOG.info(s"Flush ${p._1} -> ${p._2.size}")
            out.collect(p)
          })
          current
        }
      }
      state.update(updated)
    }
  }
  //
  val sameLabel$: DataStream[(String, Vector[Point])] = pointsTraining$
    .map(p => p.map(x => (x._1, Vector(x._2))))
    .keyBy(p => p.map(x => x._1).toString)
    // .countWindow(1.0E+6.toInt)
    // .timeWindow(Time.seconds(1))
    .process(new Transformer)
    // .reduce((a, b) => a.++(b))

  val cluster$ = sameLabel$
    .keyBy(l => l._1)
    .flatMap(list => {
      val points = list._2
      val label = list._1
      LOG.info(s"Label '$label' contains ${points.size} data points.")
      val clusters = KMeansVector.kmeans(label, k, points, iterations, varianceThreshold)
      Seq((label, clusters))
    })
  cluster$.writeAsCsv(outFilePath + "/stream-clusters.csv", FileSystem.WriteMode.OVERWRITE)
  cluster$.writeAsText(outFilePath + "/stream-clusters.txt", FileSystem.WriteMode.OVERWRITE)

  streamEnv.execute("base centroids stream")
  Thread.sleep(Time.seconds(10).toMilliseconds)
}
