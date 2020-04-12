package br.ufscar.dc.ppgcc.gsdr.minas.kmeans

import org.json._

object Point {
  def fromJson(src: JSONObject): Point = {
    val id = src.getLong("id")
    val valueJson = src.getJSONArray("value")
    val value = (0 to valueJson.length()).map(i => valueJson.getDouble(i))
    val time = src.getLong("time")
    new Point(id, value, time)
  }

  def zero(dimension: Int = 34) = Point(0, Vector.fill[Double](dimension)(0.0))
  def max(dimension: Int = 34) = Point(Long.MaxValue, Vector.fill[Double](dimension)(1.0))
  val csv: String = s"id,value,time"

  def fromCsv(csv: String): Point = {
    val split: Array[String] = csv.split(",")
    split match {
      case Array(id, value, time) => Point(id.toLong, value.split(";").map(_.toDouble).toSeq, time.toLong)
      case _ => Point.zero()
    }
  }

  trait DistanceOperator {
    def compare(x: Point, y: Point): Double
  }
  implicit object EuclideanDistance extends DistanceOperator {
    override def compare(x: Point, y: Point): Double =
      x.euclideanDistance(y)
  }
  object EuclideanSqrDistance extends DistanceOperator {
    override def compare(x: Point, y: Point): Double =
      x.euclideanSqrDistance(y)
  }
  object TaxiCabDistance extends DistanceOperator {
    override def compare(x: Point, y: Point): Double =
      x.taxiCabDistance(y)
  }
  object CosDistance extends DistanceOperator {
    override def compare(x: Point, y: Point): Double =
      x.cosDistance(y)
  }

  class PointPOJO(val id: Long, val value: Array[Double], val time: Long) {}
}
case class Point(id: Long, value: Seq[Double], time: Long = System.currentTimeMillis()) {
  def json: JSONObject = new JSONObject(Map("id"-> id, "value" -> new JSONArray(value.toArray), "time" -> time))

  lazy val csv: String = s"$id,${value.tail.foldLeft(value.head.toString)((acc, i) => s"$acc;${i.toString}")},$time"

  lazy val dimension: Int = this.value.size
  def fromOrigin(implicit distanceOperator: Point.DistanceOperator): Double = this.distance(Point.zero(this.dimension))

  def +(other: Point): Point =
    checkSize(other, Point(this.id, this.value.zip(other.value).map(x => x._1 + x._2)))

  /**
   * @param other
   * @return Internal Product = a * b = [a_i * b_i, ...]
   */
  def *(other: Point): Double =
    checkSize(other, this.value.zip(other.value).map(x => x._1 * x._2).sum)
  def *(scalar: Double): Point =
    Point(this.id, this.value.map(x => x * scalar))
  def /(scalar: Double): Point =
    this * (1/scalar)
  def unary_- : Point =
    this * -1
  def -(other: Point): Point =
    this + (-other)
  /**
   * @return ∑(aᵢ²)
   */
  def unary_| : Double =
    this.value.map(x => Math.pow(x, 2)).sum
  /**
   * @return ||point|| = √(|a|)
   */
  def unary_|| : Double =
    Math.sqrt(this.unary_|)

  override def equals(other: Any): Boolean = other match {
    case Point(id, value, time) => id == this.id && value.equals(this.value)
    case _ => false
  }

  def checkSize[T](other: Point, t: => T): T =
    if (this.value.size != other.value.size) {
      throw new RuntimeException(s"Mismatch dimensions. This is ${this.dimension} and other is ${other.dimension}.")
    } else t

  def euclideanDistance(other: Point): Double =
    checkSize(other, (this - other).unary_||)
  def euclideanSqrDistance(other: Point): Double =
    checkSize(other, (this - other).unary_|)
  def taxiCabDistance(other: Point): Double =
    checkSize(other, (this - other).value.map(d => d.abs).sum)
  def cosDistance(other: Point): Double =
    checkSize(other, (this * other) / (this.unary_|| * other.unary_||))
  def distance(other: Point)(implicit distanceOperator: Point.DistanceOperator): Double =
    distanceOperator.compare(this, other)
}
