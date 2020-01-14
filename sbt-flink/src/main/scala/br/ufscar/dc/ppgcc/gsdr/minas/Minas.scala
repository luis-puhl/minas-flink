package br.ufscar.dc.ppgcc.gsdr.minas

import br.ufscar.dc.ppgcc.gsdr.minas.helpers.ToVectAggregateFunction
import br.ufscar.dc.ppgcc.gsdr.minas.kmeans._
import grizzled.slf4j.Logger
import org.apache.flink.api.common.state.{MapStateDescriptor, ValueStateDescriptor}
import org.apache.flink.api.common.typeutils.TypeSerializer
import org.apache.flink.api.scala._
import org.apache.flink.api.scala.DataSet
import org.apache.flink.api.scala.typeutils.ScalaCaseClassSerializer
import org.apache.flink.streaming.api.functions.co.KeyedBroadcastProcessFunction
import org.apache.flink.streaming.api.scala.DataStream
import org.apache.flink.streaming.api.windowing.time.Time
import org.apache.flink.util.Collector

case class Minas(config: Map[String, Int], afterConsumedHook: ((Option[String], Point, Cluster, Double)) => Unit, distanceOp: Point.DistanceOperator) {
  // val LOG = Logger(getClass)

  lazy val k: Int = config.getOrElse("k", 100)
  lazy val noveltyDetectionThreshold: Int = config.getOrElse("noveltyDetectionThreshold", 1000)
  lazy val representativeThreshold: Int = config.getOrElse("representativeThreshold", 10)
  lazy val noveltyIndex: Int = config.getOrElse("noveltyIndex", 0)
  implicit val distanceOperator: Point.DistanceOperator = distanceOp

  // def afterConsumedHook(arg: (Option[String], Point, Cluster, Double)): Unit = {
  //   // LOG.info(s"afterConsumedHook $arg")
  // }

  def offline(sourcePoints: Seq[(String, Point)]): MinasModel = {
    val model: Vector[Cluster] = sourcePoints.groupBy(p => p._1).values.flatMap(values => {
      val label = values.head._1
      val points = values.map(_._2).toVector
      val initialClusters = Kmeans.kmeansInitialRandom(label, k, points)
      val clusters: Vector[Cluster] = Kmeans.kmeans(label, points, initialClusters)
      clusters
    }).toVector
    MinasModel(model, sleep = Vector(), noMatch = Vector(), config, e => afterConsumedHook(e))
  }
  def offline(sourcePoints: DataSet[(String, Point)]): DataSet[MinasModel] = {
    sourcePoints.groupBy(p => p._1)
    .reduceGroup(items => {
      val values = items.toVector
      val label = values.head._1
      val points = values.map(_._2)
      val initialClusters = Kmeans.kmeansInitialRandom(label, k, points)
      val clusters: Vector[Cluster] = Kmeans.kmeans(label, points, initialClusters)
      clusters
    })
    .groupBy(_ => 0)
    .reduceGroup(all => all.flatten.toVector)
    .map(model => MinasModel(model, sleep = Vector(), noMatch = Vector(), config, e => afterConsumedHook(e)))
  }
  def offline(sourcePoints: DataStream[(String, Point)]): DataStream[MinasModel] = {
    sourcePoints.keyBy(p => p._1)
      .timeWindow(Time.milliseconds(100))
      .aggregate(new ToVectAggregateFunction)
      .flatMap(map => map.toVector)
      .map(agg => {
        val label = agg._1
        val points = agg._2
        (label, points.head.id, points)
       })
      .keyBy(p => p._1 + p._2)
      .map(labelPoints => {
        val label = labelPoints._1
        val points = labelPoints._3.sortBy(p => p.fromOrigin)
        val initialClusters = Kmeans.kmeansInitialRandom(label, k, points)
        val clusters: Vector[Cluster] = Kmeans.kmeans(label, points, initialClusters)
        clusters
      })
      .keyBy(_ => "0")
      .timeWindow(Time.milliseconds(100))
      .reduce((a, b) => a ++ b)
      .keyBy(_ => 0)
      .mapWithState[MinasModel, Vector[Cluster]]((values, state) => {
        val clusters: Vector[Cluster] = values ++ state.getOrElse(Vector())
        val model = MinasModel(clusters, sleep = Vector(), noMatch = Vector(), config, e => afterConsumedHook(e))
        (model, Some(clusters))
      })
  }

  def online(minasModel: MinasModel, testPoints: Seq[Point]): (MinasModel, Seq[(Point, Option[String])]) = {
    var model = minasModel
    val consumed = testPoints.map(point => {
      val (label, next) = minasModel.consume(point)
      // LOG.info(s"afterConsumedHook $label -> $point")
      model = next
      (point, label)
    })
    (model, consumed)
  }
  def online(minasModel: MinasModel, testPoints: DataStream[Point]): DataStream[(Point, Option[String])] = {
    testPoints
      .keyBy(_ => 0)
      .mapWithState[(Point, Option[String]), MinasModel]((point, state) => {
        val modelState: MinasModel = state.getOrElse(minasModel)
        val (label, next) = modelState.consume(point)
        // LOG.info(s"afterConsumedHook $label -> $point")
        ((point, label), Some(next))
      })
  }
  /*
  garbage, flink does not help.
  def online(minasModel: DataStream[MinasModel], testPoints: DataStream[Point]): DataStream[(Point, Option[String])] = {
    // val minasModelSerializer = new MinasModelSerializer
    val minasModelState = new MapStateDescriptor[Int, MinasModel]("minasModelState", Class[Int].asInstanceOf[Class[Int]], Class[MinasModel])
    testPoints
      .keyBy(_ => 0)
      .connect(minasModel.broadcast(minasModelState))
      .process(new KeyedBroadcastProcessFunction[Int, Point, MinasModel, (Point, Option[String])] {
        override def processElement(
          value: Point,
          ctx: KeyedBroadcastProcessFunction[Int, Point, MinasModel, (Point, Option[String])]#ReadOnlyContext,
          out: Collector[(Point, Option[String])]
        ): Unit = {
          val model: MinasModel = ctx.getBroadcastState(minasModelState).get(ctx.getCurrentKey)
          val (label, next) = model.consume(value)
          out.collect((value, label))
        }

        override def processBroadcastElement(
          value: MinasModel,
          ctx: KeyedBroadcastProcessFunction[Int, Point, MinasModel, (Point, Option[String])]#Context,
          out: Collector[(Point, Option[String])]
        ): Unit = {

        }
      })
      .mapWithState((point, state) => {
        val modelState: MinasModel = state.getOrElse(minasModel)
        val (label, next) = modelState.consume(point)
        ((point, label), Some(next))
      })
  }
   */
}
