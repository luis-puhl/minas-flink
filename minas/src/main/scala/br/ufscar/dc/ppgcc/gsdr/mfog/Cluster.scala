package br.ufscar.dc.ppgcc.gsdr.mfog

import org.apache.flink.api.common.typeinfo.TypeInformation
import org.apache.flink.api.scala._
import org.json._
import scala.collection.JavaConverters._

object Cluster {
  implicit val mfogClusterTypeInfo: TypeInformation[Cluster] = createTypeInformation[Cluster]
  val CATEGORY_NORMAL = "normal"
  val CATEGORY_EXTENSION = "extension"
  val CATEGORY_NOVELTY = "novelty"
  val CATEGORY_NOVELTY_EXTENSION = "novelty extension"

  val CSV_HEADER: String = "id,label,category,matches,time,variance,center"
  // size,lblClasse,category,time,meanDistance,radius,center

  def fromJson(json: JSONObject): Cluster = {
    val id = json.getLong("id")
    val variance = json.getDouble("variance")
    val label = json.getString("label")
    val category = json.getString("category")
    val matches = json.getLong("matches")
    val time = json.getLong("time")
    val centerSrc = json.getJSONObject("time")
    val center = Point.fromJson(centerSrc)
    new Cluster(id, center, variance, label, category, matches, time)
  }

  def fromMinasCsv(line: String): Cluster =
    line.replaceAll("[\\[\\]]", "").split(",").toList match {
    case idString :: label :: category :: matches :: timeString :: meanDistance :: radius :: center => {
      val id = idString.toLong
      val time = timeString.toLong
      val cl = new Point(id, value = center.map(x => x.toDouble), time)
      Cluster(id, cl, variance = radius.toDouble, label, category, matches.toLong, time)
    }

  }
}
case class Cluster(id: Long, center: Point, variance: Double, label: String, category: String = Cluster.CATEGORY_NORMAL,
                   matches: Long = 0, time: Long = System.currentTimeMillis()) {
  def csv: String = s"$id,$label,$category,$matches,$time,$variance,[${center.value.map(_.toString).reduce(_+_)}]"
  def csvTuple: (Long, String, String, Long, Long, Double, Seq[Double]) = (id, label, category, matches, time, variance, center.value)

  def json: JSONObject = {
    new JSONObject(Map("id" -> id, "center" -> center.json, "variance" -> variance, "label" -> label,
      "category" -> category, "matches" -> matches, "time" -> time).asJava)
  }

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
