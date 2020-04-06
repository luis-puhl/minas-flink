package br.ufscar.dc.ppgcc.gsdr.minas.kmeans

import java.util
import java.util.{Arrays, Objects}

object Cluster {
  val CATEGORY_NORMAL = "normal"
  val CATEGORY_EXTENSION = "extension"
  val CATEGORY_NOVELTY = "novelty"
  val CATEGORY_NOVELTY_EXTENSION = "novelty extension"

  val CSV_HEADER: String = "id,label,category,matches,time,variance,center"
  // size,lblClasse,category,time,meanDistance,radius,center
}
case class Cluster(id: Long, center: Point, variance: Double, label: String, category: String = Cluster.CATEGORY_NORMAL,
                   matches: Long = 0, time: Long = System.currentTimeMillis()) {
  def csv: String = s"$id,$label,$category,$matches,$time,$variance,[${center.value.map(_.toString).reduce(_+_)}]"
  def csvTuple: (Long, String, String, Long, Long, Double, Seq[Double]) = (id, label, category, matches, time, variance, center.value)

  override def equals(obj: Any): Boolean =
    obj match {
      case cluster: Cluster => this.center.distance(cluster.center) < 10e-10
      case _ => super.equals(obj)
    }

  def consume(point: Point): Cluster =
    Cluster(id, center, variance, label, category, matches + 1L, System.currentTimeMillis())
  def consumeWithDistance(point: Point, distance: Double, weight: Int): Cluster =
    Cluster(id, center + (point / weight), variance max distance, label, category, matches + 1L, System.currentTimeMillis())
  def replaceLabel(label: String): Cluster =
    Cluster(id, center, variance, label, category, matches, System.currentTimeMillis())
  def replaceCenter(center: Point): Cluster =
    Cluster(id, center, variance, label, category, matches, System.currentTimeMillis())
  def replaceVariance(variance: Double): Cluster =
    Cluster(id, center, variance, label, category, matches, System.currentTimeMillis())
}
