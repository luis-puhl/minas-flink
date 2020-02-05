package br.ufscar.dc.ppgcc.gsdr.minas.kmeans

case class Cluster(id: Long, center: Point, variance: Double, label: String, matches: Long = 0, time: Long = System.currentTimeMillis()) {
  def consume(point: Point): Cluster =
    Cluster(id, center, variance, label, matches + 1L, System.currentTimeMillis())
  def consumeWithDistance(point: Point, distance: Double, weight: Int): Cluster =
    Cluster(id, center + (point / weight), variance max distance, label, matches + 1L, System.currentTimeMillis())
  def replaceLabel(label: String): Cluster =
    Cluster(id, center, variance, label, matches, System.currentTimeMillis())
  def replaceCenter(center: Point): Cluster =
    Cluster(id, center, variance, label, matches, System.currentTimeMillis())
  def replaceVariance(variance: Double): Cluster =
    Cluster(id, center, variance, label, matches, System.currentTimeMillis())
}
