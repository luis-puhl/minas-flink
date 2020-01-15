package br.ufscar.dc.ppgcc.gsdr.minas

import br.ufscar.dc.ppgcc.gsdr.minas.helpers.VectorStatistics
import br.ufscar.dc.ppgcc.gsdr.minas.kmeans._

case class MinasModel(model: Vector[Cluster], sleep: Vector[Cluster], noMatch: Vector[Point], config: Map[String, Int],
                      distanceOp: Point.DistanceOperator = Point.EuclideanSqrDistance) {
  lazy val matchCount: Map[String, Long] = {
    def toMap(m: Vector[Cluster]): Map[String, Long] =
      m.groupBy(c => c.label).map(cl => cl._1 -> cl._2.map(_.matches).sum)
    val modelCount = toMap(model)
    val sleepCount = toMap(sleep)
    (modelCount.keySet ++ sleepCount.keySet).map(key => key -> (modelCount(key) + sleepCount(key))).toMap
  }
  lazy val allClusters: Vector[Cluster] = model ++ sleep
  //
}
