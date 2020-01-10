package br.ufscar.dc.ppgcc.gsdr.minas.kmeans

object Point {
  val zero = Point(0, Vector.fill[Double](34)(0.0))
}
case class Point(id: Long, value: Vector[Double]) {
  def +(other: Point): Point =
    checkSize(other, Point(this.id, this.value.zip(other.value).map(x => x._1 + x._2)))
  def *(scalar: Double): Point =
    Point(this.id, this.value.map(x => x * scalar))
  def /(scalar: Double): Point =
    this * (1/scalar)
  def unary_- : Point =
    this * -1
  def -(other: Point): Point =
    this + (-other)

  override def equals(other: Any): Boolean = other match {
    case Point(id, value) => id == this.id
    case _ => false
  }

  def checkSize[T](other: Point, t: => T): T =
    if (this.value.size != other.value.size) {
      throw new RuntimeException(s"Mismatch dimensions. This is ${this.value.size} and other is ${other.value.size}.")
    } else t


  def euclideanDistance(other: Point): Double =
    checkSize(other, Math.sqrt(
      this.value.zip(other.value).map(x => Math.pow(x._1 - x._2, 2)).sum
    ))
  lazy val fromOrigin: Double = this.euclideanDistance(Point.zero)
}