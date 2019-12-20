package br.ufscar.dc.ppgcc.gsdr.minas.kmeans

object Point {
  val zero = Point(Vector.fill[Double](34)(0.0))
}
case class Point(value: Vector[Double]) {
  def div(value: Long): Point = Point(this.value.map(x => x / value))

  def checkSize[T](other: Point, t: => T): T =
    if (this.value.size != other.value.size) {
      throw new RuntimeException(s"Mismatch dimensions. This is ${this.value.size} and other is ${other.value.size}.")
    } else t
  def add(other: Point): Point =
    checkSize(other, Point(this.value.zip(other.value).map(x => x._1 + x._2)))

  def euclideanDistance(other: Point): Double =
    checkSize(other, Math.sqrt(
      this.value.zip(other.value).map(x => Math.pow(x._1 - x._2, 2)).sum
    ))
  def d(other: Point): Double = euclideanDistance(other)
  lazy val fromOrigin: Double = this.d(Point.zero)
}