package br.ufscar.dc.ppgcc.gsdr.minas

import br.ufscar.dc.ppgcc.gsdr.minas.helpers.VectorStatistics
import br.ufscar.dc.ppgcc.gsdr.minas.kmeans._

case class MinasModel(model: Vector[Cluster], sleep: Vector[Cluster], noMatch: Vector[Point], config: Map[String, Int],
                      afterConsumedHook: ((Option[String], Point, Cluster, Double)) => Unit) {
  lazy val matchCount: Map[String, Long] = {
    def toMap(m: Vector[Cluster]): Map[String, Long] =
      m.groupBy(c => c.label).map(cl => cl._1 -> cl._2.map(_.matches).sum)
    val modelCount = toMap(model)
    val sleepCount = toMap(sleep)
    (modelCount.keySet ++ sleepCount.keySet).map(key => key -> (modelCount(key) + sleepCount(key))).toMap
  }
  lazy val allClusters: Vector[Cluster] = model ++ sleep
  lazy val k: Int = config.getOrElse("k", 100)
  lazy val noveltyDetectionThreshold: Int = config.getOrElse("noveltyDetectionThreshold", 1000)
  lazy val representativeThreshold: Int = config.getOrElse("representativeThreshold", 10)
  lazy val noveltyIndex: Int = config.getOrElse("noveltyIndex", 0)

  def classify(point: Point, model: Vector[Cluster] = this.model, afterClassifyHook: ((Option[String], Point, Cluster, Double)) => Unit = (_ => Unit))
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

  def consume(point: Point): (Option[String], MinasModel) = {
    val classified = classify(point, this.model, this.afterConsumedHook)
    val updatedMinas = classified match {
      case Some((label, point, cluster, distance)) => {
        val modelUpdate = model
          .filter(c => c.id != cluster.id)
          .:+(cluster.consume(point))
        MinasModel(modelUpdate, sleep, noMatch, config, afterConsumedHook)
      }
      case None => MinasModel(model, sleep, noMatch :+ point, config, afterConsumedHook)
    }
    (classified.map(i => i._1), updatedMinas)
  }

  /**
   * Takes the points that din't match (short memory) and tries to match on the sleep memory.
   * If cluster matches, move it to the main model
   * @return
   */
  def rebalanced: MinasModel = {
    if (noMatch.size < noveltyDetectionThreshold) this
    else {
      val matches: Vector[(String, Point, Cluster, Double)] = noMatch.flatMap(p => classify(p, this.sleep) match {
        case Some(x) => Vector(x)
        case None => Vector()
      })
      val clusters: Vector[Cluster] = matches.foldLeft(Vector[Cluster]())((clusters, mat) => {
        clusters.filterNot(c => c.id == mat._3.id) :+ clusters.find(c => c.id == mat._3.id).getOrElse(mat._3).consume(mat._2)
      })
      val points: Vector[Point] = matches.map(_._2)
      // promote
      val updatedModel = this.model ++ clusters
      val updatedSleep = this.sleep.filterNot(c => clusters.contains(c))
      val updatedNoMatch = this.noMatch.filterNot(p => points.contains(p))
      MinasModel(updatedModel, updatedSleep, updatedNoMatch, config, afterConsumedHook)
    }
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
  def isCohesive(cluster: Cluster, distances: Vector[(Point, Double)], closest: (Point, Cluster, Double)): Boolean = {
    val simplifiedSilhouette = {
      val b: Double = closest._3
      val a: Double = VectorStatistics.VectorStdDev(distances.map(p => p._2)).stdDev
      (b - a) / (b max a)
    }
    simplifiedSilhouette > 0
  }
  def isRepresentative(cluster: Cluster, distances: Vector[(Point, Double)]): Boolean =
    representativeThreshold <= distances.size

  def noveltyDetection(implicit distanceOperator: Point.DistanceOperator) = {
    val novelty = "Novelty Flag"
    val points: Vector[Point] = this.noMatch
    val initialClusters = Kmeans.kmeansInitialRandom("noveltyDetection", k, points)
    val clusters: Vector[Cluster] = Kmeans.kmeans("noveltyDetection", points, initialClusters)
    val clustersFilled: Map[Cluster, (Vector[(Point, Double)], () => (Point, Cluster, Double))] =
        Kmeans.groupByClosest(points, clusters)
        .map(i => i._1 -> (i._2, () => Kmeans.closestCluster(i._1.center, allClusters)))
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
        val (newCenter, nearestCluster, distanceNC) = Kmeans.closestCluster(cluster.center, resultClusters)
        if (nearestCluster.variance > distanceNC)
          (noveltyCounter, resultClusters :+ cluster.replaceLabel(nearestCluster.label))
        else
          (noveltyCounter + 1, resultClusters :+ cluster.replaceLabel(s"Novelty $noveltyCounter"))
      }
    })
    val nextMinas = MinasModel(this.model ++ finalClusters, this.sleep, Vector[Point](), this.config.updated("noveltyIndex", noveltyCounter), this.afterConsumedHook)
    points.foldLeft(nextMinas)((minas, point) => minas.consume(point)._2)
  }
}
