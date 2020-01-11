package br.ufscar.dc.ppgcc.gsdr.minas.kmeans

case class Cluster(id: Long, center: Point, variance: Double, label: String, matches: Long = 0, time: Long = System.currentTimeMillis()) {
  def consume(point: Point) =
    Cluster(id, center, variance, label, matches + 1L, System.currentTimeMillis())
  def replaceLabel(label: String) =
    Cluster(id, center, variance, label, matches, System.currentTimeMillis())
}
