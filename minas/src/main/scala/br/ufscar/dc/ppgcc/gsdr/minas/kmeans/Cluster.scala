package br.ufscar.dc.ppgcc.gsdr.minas.kmeans

object Cluster {
  val CSV_HEADER: String = "id,label,matches,time,variance,center"
}
case class Cluster(id: Long, center: Point, variance: Double, label: String, matches: Long = 0, time: Long = System.currentTimeMillis()) {
  def csv: String = s"$id,$label,$matches,$time,$variance,[${center.value.map(_.toString).reduce(_+_)}]"
  def csvTuple: (Long, String, Long, Long, Double, Seq[Double]) = (id, label, matches, time, variance, center.value)

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
