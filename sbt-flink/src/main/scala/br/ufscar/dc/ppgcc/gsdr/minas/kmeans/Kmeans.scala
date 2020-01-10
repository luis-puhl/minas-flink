package br.ufscar.dc.ppgcc.gsdr.minas.kmeans

import grizzled.slf4j.Logger

object Kmeans {
  val LOG = Logger(getClass)

  // case class MicroCluster(id: Long, center: Point, contents: Option[Vector[Point]] = None, distances: Option[Vector[(Point, Double)]] = None, variance: Option[Double] = None)

  // case class MacroCluster(root: Cluster, children: Vector[Cluster])

  def groupByClosest(points: Vector[Point], clusters: Vector[Cluster]): Map[Cluster, Vector[(Point, Double)]] = {
    points.map(p => clusters.map(c => (p, c, c.center.euclideanDistance(p))).minBy(d => d._3))
      .groupBy(d => d._2).map(i => i._1 -> i._2.map(d => (d._1, d._3)))
  }

  def updateClustersVariance(clusterDistanceMap: Map[Cluster, Vector[(Point, Double)]]): Vector[Cluster] = {
    clusterDistanceMap.map(sameCluster => {
      val cluster = sameCluster._1
      val variance = sameCluster._2.map(p => p._2).max
      Cluster(cluster.id, cluster.center, variance)
    }).toVector
  }

  def updateClustersCenters(clusterDistanceMap: Map[Cluster, Vector[(Point, Double)]]): Vector[(Cluster, Double)] = {
    clusterDistanceMap.map(sameCluster => {
      val cluster = sameCluster._1
      val points = sameCluster._2.map(p => p._1)
      val variance = sameCluster._2.map(p => p._2).max
      val center = points.reduce((a, b) => a + b) / points.size
      val movement = cluster.center.euclideanDistance(center)
      // LOG.info(s"Cluster ${cluster.id} moved $movement => $center")
      // (Cluster(cluster.id, center, Some(points), Some(sameCluster._2), Some(variance)), movement)
      (Cluster(cluster.id, center, variance), movement)
    }).toVector
  }

  def kmeans(points: Vector[Point], clusters: Vector[Cluster], prevMovement: Double = Double.MaxValue, targetImprovement: Double = 10E-5, limit: Int = 10, i: Int = 0): Vector[Cluster] = {
    if (i > limit) clusters
    else {
      val distances = groupByClosest(points, clusters)
      if (distances.keys.size != clusters.size) {
        LOG.warn(s"Reduction in clusters array. Had ${clusters.size} and got ${distances.keys.size}. i=$i, target=$targetImprovement")
      }
      val newClusters = updateClustersCenters(distances)
      val totalMovement = newClusters.map(c => c._2).sum
      val improvement = totalMovement / prevMovement
      val onlyNewClusters = newClusters.map(c => c._1)
      LOG.info(s"kmeans c=${clusters.size} p=${points.size} i=$i / $limit, movement=$improvement / $targetImprovement")
      if (improvement < targetImprovement) onlyNewClusters
      else kmeans(points, onlyNewClusters, totalMovement, targetImprovement, limit, i + 1)
    }
  }
}
