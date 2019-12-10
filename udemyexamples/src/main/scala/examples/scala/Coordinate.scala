package examples.scala

/**
 * Common trait for operations supported by both points and centroids
 * Note: case class inheritance is not allowed in Scala
 */
trait Coordinate extends Serializable {

  var x: Double
  var y: Double

  def add(other: Coordinate): this.type = {
    x += other.x
    y += other.y
    this
  }

  def div(other: Long): this.type = {
    x /= other
    y /= other
    this
  }

  def euclideanDistance(other: Coordinate): Double =
    Math.sqrt((x - other.x) * (x - other.x) + (y - other.y) * (y - other.y))

  def clear(): Unit = {
    x = 0
    y = 0
  }

  override def toString: String =
    s"$x $y"

}

/**
 * A simple two-dimensional point.
 */
case class Point(var x: Double = 0, var y: Double = 0) extends Coordinate

/**
 * A simple two-dimensional centroid, basically a point with an ID.
 */
case class Centroid(var id: Int = 0, var x: Double = 0, var y: Double = 0) extends Coordinate {
  def this(id: Int, p: Point) {
    this(id, p.x, p.y)
  }

  override def toString: String = s"$id ${super.toString}"
}