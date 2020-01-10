package br.ufscar.dc.ppgcc.gsdr.minas.kmeans

object Point {
  def zero(dimension: Int = 34) = Point(0, Vector.fill[Double](dimension)(0.0))

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
}
case class Point(id: Long, value: Vector[Double]) {
  lazy val dimension: Int = this.value.size
  lazy val fromOrigin: Double = this.distance(Point.zero(this.dimension))

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
    case Point(id, value) => id == this.id && value.equals(this.value)
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
  def distance(other: Point)(implicit distance: Point.DistanceOperator): Double =
    distance.compare(this, other)
}
