package br.ufscar.dc.ppgcc.gsdr.minas

import br.ufscar.dc.ppgcc.gsdr.minas.datasets.kdd._
import br.ufscar.dc.ppgcc.gsdr.minas.kmeans._
import br.ufscar.dc.ppgcc.gsdr.minas.kmeans.KMeansVector

import grizzled.slf4j.Logger

import org.apache.flink.api.common.functions.RichMapFunction
import org.apache.flink.api.common.state.{ValueState, ValueStateDescriptor}
import org.apache.flink.api.scala.{DataSet, ExecutionEnvironment, extensions}
import org.apache.flink.streaming.api.scala._
import org.apache.flink.streaming.api.windowing.time.Time
import org.apache.flink.core.fs.FileSystem
import org.apache.flink.streaming.api.functions.KeyedProcessFunction
import org.apache.flink.streaming.api.windowing.assigners.WindowAssigner
import org.apache.flink.util.Collector

// import moa.clusterers.KMeans
// import moa.clusterers.clustream.Clustream

import scala.collection._
import scala.concurrent.duration._

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
//  setEnv.execute("base centroids")


//  val optionTrainingVector: Seq[Option[KddCassalesEntry]] = trainingSet
//    .map(p => Option(p))
//    .collect()
//    .++(Seq(Option.empty[KddCassalesEntry]))
//  val training$: DataStream[Option[KddCassalesEntry]] = streamEnv.fromCollection(optionTrainingVector)
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
  training$2.writeAsText(outFilePath + "/stream-ini.csv", FileSystem.WriteMode.OVERWRITE)
  //
  case class MicroCluster(id: Long, center: Point, contents: Option[Vector[Point]] = None, distances: Option[Vector[(Point, Double)]] = None, variance: Option[Double] = None) {
    def withMedian: Option[MicroCluster] = {
      val center: Option[Point] = contents.map(c => c.reduce((a, b) => a.add(b)).div(contents.size))
      val distances: Option[Vector[(Point, Double)]] = for {
        c <- center
        points <- contents
      } yield points.map(p => (p, c.euclideanDistance(p)))
      val variance = distances.map(ds => ds.maxBy(d => d._2)._2)
      for {
        c <- center
        d <- distances
        v <- variance
      } yield MicroCluster(id, c, contents, Some(d), Some(v))
    }
  }
  case class MacroCluster(root: Cluster, children: Vector[Cluster])
  def kmeans(points: Vector[Point], clusters: Vector[MicroCluster], target: Double = 10E-5, limit: Int = 10, i: Int = 0): Vector[MicroCluster] = {
    if (i > limit) clusters
    else {
      val variance = clusters.map(c => c.variance.get).sum
      val distances = points.map(p => clusters.map(c => (p, c, p.euclideanDistance(c.center))).minBy(d => d._3))
      val byCluster = distances.groupBy(d => d._2.id).values
      if (byCluster.size != clusters.size) {
        LOG.info(s"Reduction in clusters array. Had ${clusters.size} and got ${byCluster.size}")
      }
      val newClusters = byCluster.map(sameCluster => {
        val cluster = sameCluster.head._2
        val points = sameCluster.map(d => d._1)
        MicroCluster(cluster.id, cluster.center, Some(points)).withMedian.get
      }).toVector
      val newVariance = newClusters.map(c => c.variance.get).sum
      if (newVariance / variance < target) newClusters
      else kmeans(points, newClusters, target, limit, i + 1)
    }
  }
  //

  class CountWithTimeoutFunction[K, T](count: Long, time: Long) extends KeyedProcessFunction[K, T, Vector[T]] {
    case class CountWithTimestamp(key: K, values: Vector[T], lastModified: Long)
    lazy val state: ValueState[CountWithTimestamp] = getRuntimeContext
      .getState(new ValueStateDescriptor[CountWithTimestamp]("CountWithTimeoutFunction", classOf[CountWithTimestamp]))
    override def processElement(value: T, ctx: KeyedProcessFunction[K, T, Vector[T]]#Context, out: Collector[Vector[T]]): Unit = {
      val key = ctx.getCurrentKey
      val timestamp: Long = if (ctx != null) {
        if (ctx.timestamp != null) ctx.timestamp
      else if (ctx != null && ctx.timerService != null && ctx.timerService.currentProcessingTime != null) ctx.timerService.currentProcessingTime
      else 0
      val current: CountWithTimestamp = Option(state.value) match {
        case Some(CountWithTimestamp(key, values, lastModified)) => {
          val items = values :+ value
          val remaining = if (items.size >= count) {
            out.collect(items)
            Vector()
          } else items
          CountWithTimestamp(key, remaining, timestamp)
        }
        case None => {
          LOG.info(s"$key, $value, $timestamp")
          CountWithTimestamp(key, Vector(value), timestamp)
        }
      }
      state.update(current)
      ctx.timerService.registerEventTimeTimer(current.lastModified + time)
    }
    override def onTimer(timestamp: Long, ctx: KeyedProcessFunction[K, T, Vector[T]]#OnTimerContext, out: Collector[Vector[T]]): Unit = {
      val current = state.value match {
        case CountWithTimestamp(key, items, lastModified) if (timestamp == lastModified + time) => {
          out.collect(items)
          CountWithTimestamp(key, Vector(), ctx.timestamp)
        }
        case _ => state.value
      }
      state.update(current)
    }
  }
  //
  val clusters$ = training$2
    // .map(p => (p._1, Vector(p._2)))
    .keyBy(p => p._1)
    .process(new CountWithTimeoutFunction[String, (String, Point)](100, 1.seconds.toMillis))
    .map(agg => (agg.head._1, agg.map(p => p._2)))
    //.countWindow(100)
    //.reduce((a, b) => (a._1, a._2 ++ b._2))
    .keyBy(p => p._1)
    .map(p => {
      val label = p._1
      val points = p._2.sortBy(p => p.fromOrigin)
      LOG.info(s"Taking in ${points.size} from $label.")
      val c0 = Vector(points.head, points.last).map(p => MicroCluster(p.id, p))
      val clusters = points.map(p => c0.map(c => (p, c, p.euclideanDistance(c.center))).minBy(d => d._3))
        .groupBy(d => d._2.id).values
        .map(sameCluster => {
          val cluster = sameCluster.head._2
          val points = sameCluster.map(d => d._1)
          val distances = sameCluster.map(d => (d._1, d._3))
          val variance = distances.maxBy(d => d._2)._2
          MicroCluster(cluster.id, cluster.center, Some(points), Some(distances), Some(variance))
        }).toVector
      kmeans(points, clusters)
    })
  clusters$.writeAsText(outFilePath + "/basic-kmeans.txt", FileSystem.WriteMode.OVERWRITE)
//  val clusters$ = training$2.keyBy(p => p._1)
//      .flatMapWithState[(String, Cluster), Vector[Point]]((entry: (String, Point), state: Option[Vector[Point]]) => {
//        val label = entry._1
//        val point = entry._2
//        val points: Vector[Point] = state match {
//          case Some(points: Vector[Point]) => points.+:(point)
//          case None => Vector(point)
//        }
//        if (points.size < 2*k || points.size % 2*k != 0) {
//          (Seq[(String, Cluster)](), Some(points))
//        } else {
//          val clusters = KMeansVector.kmeans(label, k, points, iterations, varianceThreshold)
//          (clusters._1.map(c => (label, c)), Some(Vector[Point]()))
//        }
//      })
//  clusters$.writeAsText(outFilePath + "/stream-clusters.csv", FileSystem.WriteMode.OVERWRITE)
  // training$.writeAsCsv(outFilePath + "/stream-ini.csv", FileSystem.WriteMode.OVERWRITE)
  // training$.writeAsText(outFilePath + "/stream-ini.txt", FileSystem.WriteMode.OVERWRITE)
/*
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
*/
  streamEnv.execute("base centroids stream")
  Thread.sleep(Time.seconds(10).toMilliseconds)
}
