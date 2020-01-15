package br.ufscar.dc.ppgcc.gsdr.minas

import br.ufscar.dc.ppgcc.gsdr.minas.helpers.{ToVectAggregateFunction, VectorStatistics}
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

case class Minas(config: Map[String, Int], distanceOp: Point.DistanceOperator) {
   val LOG = Logger(getClass)

  protected lazy val k: Int = config.getOrElse("k", 100)
  protected lazy val noveltyDetectionThreshold: Int = config.getOrElse("noveltyDetectionThreshold", 1000)
  protected lazy val representativeThreshold: Int = config.getOrElse("representativeThreshold", 10)
  protected lazy val sleepThreshold: Int = config.getOrElse("sleepThreshold", 100)
  protected lazy val discardThreshold: Int = config.getOrElse("discardThreshold", 100)
  protected lazy val noveltyIndex: Int = config.getOrElse("noveltyIndex", 0)
  implicit val distanceOperator: Point.DistanceOperator = distanceOp

  def afterConsumedHook(arg: (Option[String], Point, Cluster, Double)): Unit = {
//    LOG.info(s"afterConsumedHook $arg")
  }
  def afterClassifyHook(arg: (Option[String], Point, Cluster, Double)): Unit = {
//    LOG.info(s"afterClassifyHook $arg")
  }

  protected def classify(point: Point, model: Vector[Cluster])
              (implicit distanceOperator: Point.DistanceOperator)
  : Option[(String, Point, Cluster, Double)] = {
    val (_, cluster, distance) = Kmeans.closestCluster(point, model)
    if (distance > cluster.variance) {
      afterClassifyHook((None, point, cluster, distance))
      None
    }
    else {
      afterClassifyHook((Some(cluster.label), point, cluster, distance))
      Some((cluster.label, point, cluster, distance))
    }
  }

  protected def consume(point: Point, minasModel: MinasModel): (Option[String], MinasModel) = {
    val classified = classify(point, minasModel.model)
    val updatedMinas = classified match {
      case Some((label, point, cluster, distance)) => {
        val modelUpdate = minasModel.model
          .filter(c => c.id != cluster.id)
          .:+(cluster.consume(point))
        MinasModel(modelUpdate, minasModel.sleep, minasModel.noMatch, config)
      }
      case None => MinasModel(minasModel.model, minasModel.sleep, minasModel.noMatch :+ point, config)
    }
    (classified.map(i => i._1), updatedMinas)
  }

  protected def next(point: Point, minasModel: MinasModel): (Seq[(Option[String], Point)], MinasModel) = {
    val (firstLabel, m0) = consume(point, minasModel)
    var consumed = Vector((firstLabel, point))
    var next = m0
    //
    val (sleepMatches, promoted) = promoteFromSleep(next)
    next = promoted
    consumed = consumed ++ sleepMatches
    //
    val (novelty, mfinal) = noveltyDetection(next)
    next = mfinal
    consumed = consumed ++ novelty
    //
    next = demoteToSleep(next)
    next = discard(next)
    //
    (consumed, next)
  }

  /**
   * Takes the points that din't match (short memory) and tries to match on the sleep memory.
   * If cluster matches, move it to the main model
   * @return
   */
  protected def promoteFromSleep(minasModel: MinasModel): (Vector[(Option[String], Point)], MinasModel) = {
    if (minasModel.noMatch.size < noveltyDetectionThreshold || minasModel.sleep.isEmpty) (Vector.empty, minasModel)
    else {
      val matches: Vector[(String, Point, Cluster, Double)] = minasModel.noMatch.flatMap(p => classify(p, minasModel.sleep) match {
        case Some(x) => Vector(x)
        case None => Vector()
      })
      val clusters: Vector[Cluster] = matches.foldLeft(Vector[Cluster]())((clusters, mat) => {
        clusters.filterNot(c => c.id == mat._3.id) :+ clusters.find(c => c.id == mat._3.id).getOrElse(mat._3).consume(mat._2)
      })
      val points: Vector[Point] = matches.map(_._2)
      // promote
      val updatedModel = minasModel.model ++ clusters
      val updatedSleep = minasModel.sleep.filterNot(c => clusters.contains(c))
      val updatedNoMatch = minasModel.noMatch.filterNot(p => points.contains(p))
      (matches.map(m => (Some(m._1), m._2)), MinasModel(updatedModel, updatedSleep, updatedNoMatch, config))
    }
  }
  protected def demoteToSleep(minasModel: MinasModel): MinasModel = {
    val currentTime = System.currentTimeMillis()
    val toSleep = minasModel.model.filter(c => currentTime - c.time > this.sleepThreshold)
    val updatedModel = minasModel.model.filterNot(c => toSleep.contains(c))
    val updatedSleep = minasModel.sleep ++ toSleep
    if (updatedModel.isEmpty)
      MinasModel(updatedSleep, updatedModel, minasModel.noMatch, minasModel.config)
    else
      MinasModel(updatedModel, updatedSleep, minasModel.noMatch, minasModel.config)
  }
  protected def discard(minasModel: MinasModel): MinasModel = {
    val earlierTime = System.currentTimeMillis() - discardThreshold
    val updatedNoMatch = minasModel.noMatch.filter(p => p.time > earlierTime)
    val discarded = minasModel.noMatch.size - updatedNoMatch.size
//    if (discarded > 0)
//      LOG.info(s"Discarding $discarded")
    MinasModel(minasModel.model, minasModel.sleep, updatedNoMatch, minasModel.config)
  }

  /**
   * A new micro-cluster is cohesive if its silhouette coefficient is larger than 0 (see Eq. 1).
   * For such, MINAS uses a simplified silhouette coefficient (Vendramin et al. 2010).
   * In Eq. 1, _b_ represents the Euclidean distance between the centroid of the new micro-cluster
   * and the centroid of its closest micro-cluster, and _a_ represents the standard deviation of the
   * distances between the examples of the new micro-cluster and the centroid of the new micro-cluster.
   * Silhouette = (b − a) / max(b, a)
   *
   * @param cluster
   * @return
   */
  protected def isCohesive(cluster: Cluster, distances: Vector[(Point, Double)], closest: (Point, Cluster, Double)): Boolean = {
    val simplifiedSilhouette = {
      val b: Double = closest._3
      val a: Double = VectorStatistics.VectorStdDev(distances.map(p => p._2)).stdDev
      (b - a) / (b max a)
    }
    simplifiedSilhouette > 0
  }
  protected def isRepresentative(cluster: Cluster, distances: Vector[(Point, Double)]): Boolean =
    representativeThreshold <= distances.size

  protected def noveltyDetection(minasModel: MinasModel)(implicit distanceOperator: Point.DistanceOperator): (Vector[(Option[String], Point)], MinasModel) = {
    if (minasModel.noMatch.size < noveltyDetectionThreshold) (Vector.empty, minasModel)
    else {
      LOG.info("noveltyDetection")
      val novelty = "Novelty Flag"
      val points: Vector[Point] = minasModel.noMatch
      val initialClusters = Kmeans.kmeansInitialRandom("noveltyDetection", k, points)
      val clusters: Vector[Cluster] = Kmeans.kmeans("noveltyDetection", points, initialClusters)
      val clustersFilled: Map[Cluster, (Vector[(Point, Double)], () => (Point, Cluster, Double))] =
        Kmeans.groupByClosest(points, clusters)
          .map(i => i._1 -> (i._2, () => Kmeans.closestCluster(i._1.center, minasModel.allClusters)))
      val clustersClassified: Iterable[Cluster] = clustersFilled
        .filter(cl => isRepresentative(cl._1, cl._2._1))
        .map(cl => (cl._1, cl._2._1, cl._2._2.apply()))
        .filter(cl => isCohesive(cl._1, cl._2, cl._3))
        .map(cl => {
          val (cluster, distances, closest) = cl
          val (newCenter, nearestCluster, distanceNC) = closest
          if (nearestCluster.variance > distanceNC) cluster.replaceLabel(nearestCluster.label)
          else cluster.replaceLabel(novelty)
        })
      val (noveltyCounter, finalClusters) = clustersClassified.foldLeft((noveltyIndex, Vector[Cluster]()))((res, cluster) => {
        val (noveltyCounter, resultClusters: Vector[Cluster]) = res
        if (cluster.label != novelty) (noveltyCounter, resultClusters :+ cluster)
        else {
          lazy val (newCenter, nearestCluster, distanceNC) = Kmeans.closestCluster(cluster.center, resultClusters)
          if (resultClusters.nonEmpty && nearestCluster.variance > distanceNC)
            (noveltyCounter, resultClusters :+ cluster.replaceLabel(nearestCluster.label))
          else
            (noveltyCounter + 1, resultClusters :+ cluster.replaceLabel(s"Novelty $noveltyCounter"))
        }
      })
      val nextMinas = MinasModel(minasModel.model ++ finalClusters, minasModel.sleep, Vector[Point](), this.config.updated("noveltyIndex", noveltyCounter))
      val initialAcc = (Vector.empty[(Option[String], Point)], nextMinas)
      points.foldLeft(initialAcc)((acc, point) => {
        val (results, minas) = acc
        val (label, next) = consume(point, minas)
        (results :+ (label, point), next)
      })
    }
  }

  /**
   * ------------------------------------------------------------------------------------------------------------------
   */

  def offline(sourcePoints: Seq[(String, Point)]): MinasModel = {
    val model: Vector[Cluster] = sourcePoints.groupBy(p => p._1).values.flatMap(values => {
      val label = values.head._1
      val points = values.map(_._2).toVector
      val initialClusters = Kmeans.kmeansInitialRandom(label, k, points)
      val clusters: Vector[Cluster] = Kmeans.kmeans(label, points, initialClusters)
      clusters
    }).toVector
    MinasModel(model, sleep = Vector(), noMatch = Vector(), config)
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
    .map(model => MinasModel(model, sleep = Vector(), noMatch = Vector(), config))
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
        val model = MinasModel(clusters, sleep = Vector(), noMatch = Vector(), config)
        (model, Some(clusters))
      })
  }

  /**
   * ------------------------------------------------------------------------------------------------------------------
   */

  def online(model: MinasModel, testPoints: Seq[Point]): (Seq[(Option[String], Point)], MinasModel) = {
    case class LocalMatch(matches: Seq[(Option[String], Point)], minasModel: MinasModel)
    val start = LocalMatch(Vector.empty, model)
    val result: LocalMatch = testPoints.foldLeft(start)((state: LocalMatch, point: Point) => {
      val (matches, nextModel) = next(point, state.minasModel)
      LocalMatch((state.matches ++ matches).toVector, nextModel)
    })
    (result.matches, result.minasModel)
  }
  def online(minasModel: MinasModel, testPoints: DataStream[Point]): DataStream[(Point, Option[String])] = {
    testPoints
      .keyBy(_ => 0)
      .mapWithState[(Point, Option[String]), MinasModel]((point, state) => {
        val modelState: MinasModel = state.getOrElse(minasModel)
        val (label, next) = consume(point, modelState)
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
