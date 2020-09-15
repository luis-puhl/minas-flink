package br.ufscar.dc.ppgcc.gsdr.minas

import br.ufscar.dc.ppgcc.gsdr.minas.datasets.kdd._
import br.ufscar.dc.ppgcc.gsdr.minas.kmeans._
import grizzled.slf4j.Logger
import org.apache.flink.api.scala.ExecutionEnvironment
import org.apache.flink.core.fs.FileSystem
import org.apache.flink.streaming.api.functions.windowing.delta.DeltaFunction
import org.apache.flink.streaming.api.scala._
import org.apache.flink.streaming.api.windowing.assigners.{GlobalWindows, WindowAssigner}
import org.apache.flink.streaming.api.windowing.time.Time
import org.apache.flink.streaming.api.windowing.triggers.{DeltaTrigger, Trigger}
import org.apache.flink.streaming.api.windowing.windows.TimeWindow
import org.apache.flink.util.Collector

object MinasKddParallelApp extends App {
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

  class ZipWithIndexResult[T](val entry: T, val index: Long)
  implicit class RichStream[T](val value: DataStream[T]) extends AnyVal {
    def zipWithIndex: DataStream[ZipWithIndexResult[T]] = value.keyBy(p => 0)
      .mapWithState[ZipWithIndexResult[T], Long]((entry: T, counterState: Option[Long]) => {
        val counter = counterState match {
          case Some(count) => count
          case None => 0L
        }
        (new ZipWithIndexResult[T](entry, counter), Some(counter + 1L))
      })
  }
//  def countOrTimeoutWindow(dataStream: DataStream[(String, Point)], count: Long, timeout: Long): DataStream[(String, Vector[Point])] =
//    dataStream
//      .map(e => (e._1, Vector(e._2)))
//      .keyBy(e => e._1 + (e._2.head.id % dataStream.parallelism))
//      .window(GlobalWindows.create())
//      .trigger(DeltaTrigger.of(1, new DeltaFunction[(String, Vector[Point])] {
//        override def getDelta(oldDataPoint: (String, Vector[Point]), newDataPoint: (String, Vector[Point])): Double =
//      }))
//    .

  val training: DataStream[(String, Point)] = streamEnv.readTextFile(inPathIni)
    .map(line => KddCassalesEntryFactory.fromStringLine(line))
    .zipWithIndex
    .map(indexed => (indexed.entry.label, Point(indexed.index, indexed.entry.value)))
  //
  LOG.info(s"training.parallelism = ${training.parallelism}")

//  def clustream(dataStream: DataStream[In]): DataStream[Out] = dataStream
//    .keyBy(e => e._2.id % training.parallelism)
//    .timeWindow(Time.minutes(1))
//    .apply[(String, Vector[Point])]((key: Long, window: TimeWindow, in: Iterable[In], out: Collector[(String, Vector[Point])]) => {
//      in.groupBy(i => i._1).foreach {
//        case (label, values) => out.collect((label, values.map(_._2).toVector))
//      }
//    })
//    .map(i => i match {
//      case (label: String, points: Vector[Point]) =>
//        LOG.info(s"$label -> ${points.size}")
//        val ini = Kmeans.kmeansInitialRandom(label, k, points)
//        LOG.info(s"ini $label -> ${ini.size}")
//        val fin = Kmeans.kmeans(label, points, ini)
//        LOG.info(s"fin $label -> ${fin.size}")
//        (label, points.head, fin)
//    })
//  clustream(training).writeAsText(outFilePath + "/clustream_training", FileSystem.WriteMode.OVERWRITE)

  class StreamingCentroid(id: Long, value: Vector[Double], val label: String, val size: Long = 1) extends Point(id, value) {
    override def +(other: Point): StreamingCentroid =
      new StreamingCentroid(id, (super.+(other) / (size + 1)).value, label, size + 1)

    override def toString: String =
      s"StreamingCentroid($label, $size, ${super.toString})"
  }
  def closestCluster(point: Point, clusters: Vector[StreamingCentroid])(implicit distanceOperator: Point.DistanceOperator): (Point, StreamingCentroid, Double) = {
    clusters.map(c => (point, c, c.distance(point))).minBy(d => d._3)
  }

  type IS = Map[StreamingCentroid, Vector[Double]]
  type S = Map[String, IS]
  val clusters = training
    .keyBy(i => i._1)
    .mapWithState[S, S]((entry: (String, Point), stateOption: Option[S]) => entry match {
      case (label: String, point: Point) =>
        val superState: S = stateOption match {
          case Some(state) => state
          case None => Map()
        }
        val state: IS = superState.getOrElse(label, Map())
        //
        val nextState: IS = if (state.size < k)
          state + (new StreamingCentroid(point.id, point.value, label) -> Vector(0.0))
        else {
          val clusters: Vector[StreamingCentroid] = state.keySet.toVector
          val (_, c, d) = closestCluster(point, clusters)
          val newCenter: StreamingCentroid = c + point
          val older: Vector[Double] = state.getOrElse(c, Vector())
          val withFilter: Map[StreamingCentroid, Vector[Double]] = state.filterKeys(cl => cl == c)
          withFilter + (newCenter -> (older :+ d))
        }
        val nextSuperState = superState.updated(label, nextState)
        (nextSuperState, Some(nextSuperState))
    })
    .map(s => s.mapValues(i => i.keys))
//  clusters
//    .writeAsText(outFilePath + "/training.offlineKMeans", FileSystem.WriteMode.OVERWRITE)
  clusters
    .map(i => i.values.flatten.toSet)
    .writeAsText(outFilePath + "/training.offlineKMeans.flatten.toSet", FileSystem.WriteMode.OVERWRITE)
  /*
  type In = (String, Point)
  type Out = (String, Point, Vector[Cluster])
  type State = Map[Cluster, Vector[(Point, Double)]]
  type SuperState = Map[String, State]
  val clusters = training
    // .offlineKMeans(k)
    // .keyBy(e => e._1)
    // .keyBy(e => e._1 + e._2.id % training.parallelism)
    .keyBy(e => e._2.id % training.parallelism)
    .mapWithState[Out, SuperState]((entry: In, stateOption: Option[SuperState]) => entry match {
      case (label: String, point: Point) => {
        val superState: SuperState = stateOption match {
          case Some(state) => state
          case None => Map()
        }
        val state: State = superState.getOrElse(label, Map())
        //
        val nextState: State = if (state.size < k)
          state + (Cluster(point.id, point, variance = 0.0, label) -> Vector((point, 0.0)))
        else {
          val clusters = state.keySet.toVector
          val (_, c, d) = Kmeans.closestCluster(point, clusters)
          val matchedPoints = state.getOrElse(c, Vector())
          val nextCenter = matchedPoints.map(_._1).reduce(_ + _) / matchedPoints.size
          val nextCluster = c.replaceCenter(nextCenter)
          clusters.filterNot(cl => cl.id == c.id) :+ nextCluster
          state.filterKeys(cl => cl.id != c.id) + (nextCluster -> (matchedPoints :+ (point, d)))
        }
        ((label, point, nextState.keySet.toVector), Some(superState.updated(label, nextState)))
      }
    })
  clusters.writeAsText(outFilePath + "/training.offlineKMeans(k)", FileSystem.WriteMode.OVERWRITE)
  */
//
//  def afterConsumedHook(arg: (Option[String], Point, Cluster, Double)): Unit = {
//    LOG.info(s"afterConsumedHook $arg")
//  }
//
//  val minas: Minas = Minas(config, afterConsumedHook)
//  val model: MinasModel = minas.offline(stream)
//
//  val testStream: DataStream[(String, Point)] = streamEnv.readTextFile(inPathOnl)
//    .map(line => KddCassalesEntryFactory.fromStringLine(line))
//    .keyBy(p => 0)
//    .mapWithState[(String, Point), Long]((entry: KddCassalesEntry, counterState: Option[Long]) => {
//      val counter = counterState match {
//        case Some(count) => count
//        case None => 0L
//      }
//      if (!entry.isNormalized) LOG.info("out of normal form")
//      ((entry.label, Point(counter, entry.value)), Some(counter + 1L))
//    })
//  testStream.writeAsText(outFilePath + "/testStream", FileSystem.WriteMode.OVERWRITE)
//
//  val result = minas.online(model, testStream.map(_._2))
//  result.writeAsText(outFilePath + "/result", FileSystem.WriteMode.OVERWRITE)

  streamEnv.execute(jobName)
}
