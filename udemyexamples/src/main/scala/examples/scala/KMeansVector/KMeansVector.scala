package examples.scala.KMeansVector

import java.nio.file.{Files, Paths}

import org.apache.flink.api.scala._
import org.apache.flink.core.fs.FileSystem
import org.apache.flink.streaming.api.scala.{DataStream, StreamExecutionEnvironment}
import org.apache.flink.util.Collector

object KMeansVector extends App {
  val streamEnv = StreamExecutionEnvironment.getExecutionEnvironment
  val setEnv = ExecutionEnvironment.getExecutionEnvironment

  val kddPath = "./kdd.ics.uci.edu/databases/kddcup99/kddcup.data"
  val tempPath = "./tmpfs/kddcup.data"
  val filePathInput = if ( Files.exists(Paths.get(tempPath)) ) tempPath else kddPath
  val outFilePath = if ( Files.exists(Paths.get("tmpfs")) ) "./tmpfs/out" else "out"
  import Kdd.{dimension, magnitude, classes, magnitudeOne10th}
  val relativeMagnitude: Int = magnitude / classes
  val relativeMagnitude10til: Int = magnitudeOne10th / classes

  final case class Point(value: Vector[Double]) extends IndexedSeq[Double] {
    def apply(i: Int): Double = value.apply(i)
    override def length: Int = value.length
    def euclideanDistance(other: Point): Double =
      if (this.value.size != other.value.size) throw new RuntimeException("Mismatch dimensions")
      else Math.sqrt(
        this.value.zip(other.value)
          .reduce((x: (Double, Double), acc: (Double, Double)) => ((x._1 - x._2) * (x._1 - x._2) + acc._1, 0.0))._1
      )
    def -(other: Point): Double = this.euclideanDistance(other)
    def +(other: Point): Point = Point(this.value.zip(other.value).map(x => x._1 + x._2))
    def *(scalar: Double): Point = Point(this.value.map(x => x * scalar))
    def /(scalar: Double): Point = this * (1/scalar)
    def unary_-  : Point = this * (-1)
  }
  final case class Centroid(label: Double, point: Point)
  def selectNearestCenter(point: Point, centroids: Seq[Centroid]): (Double, Centroid) =
    centroids.map(c => (point.euclideanDistance(c.point ), c)).reduce(
      (a, b) => if (a._1 < b._1) a else b
    )

  //
  val centroids: DataSet[Centroid] = setEnv
    .readTextFile(filePathInput + "_10_percent")
    .map(x => {
      val kdd = Kdd.fromLine(x)
      Centroid(kdd(dimension), Point(kdd.take(dimension -1).toVector))
    })
    .groupBy("label")
    .reduceGroup((values: Iterator[Centroid], out: Collector[Centroid]) =>
      values
        .grouped(relativeMagnitude10til /  10)
        .map(
          g => if (g.exists(_.label != g.head.label)) throw new RuntimeException("Mismatch labels")
          else Centroid(g.head.label, g.map(_.point).reduce((a, b) => a + b) / g.size)
        )
      .foreach(out.collect)
    )
  println(centroids.collect())
//  centroids.writeAsText(outFilePath + "/centroids", FileSystem.WriteMode.OVERWRITE)
//  setEnv.execute("base centroids")

//    .reduce((a, b) =>
//      if (a.label != b.label) throw new RuntimeException("Mismatch labels")
//      else Centroid(a.label, (a.point + b.point) / 2)
//    )

//  tenPercentKdd.countWindowAll(100).reduce()
//
//  val fullKdd: DataStream[Seq[Double]] = streamEnv
//    .readTextFile(filePathInput)
//    .map(x => Kdd.fromLine(x).take(dimension))
//
//  item$.writeAsText(outFilePath, FileSystem.WriteMode.OVERWRITE)
//  streamEnv.execute()
}
