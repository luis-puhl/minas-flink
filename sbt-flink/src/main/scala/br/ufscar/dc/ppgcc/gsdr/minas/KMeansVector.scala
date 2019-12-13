package br.ufscar.dc.ppgcc.gsdr.minas

import java.nio.file.{Files, Paths}
import java.util

import org.apache.flink.api.common.functions.RichMapFunction
import org.apache.flink.api.scala._
import org.apache.flink.configuration.Configuration
import org.apache.flink.core.fs.FileSystem
import org.apache.flink.streaming.api.scala.{DataStream, StreamExecutionEnvironment}
import org.apache.flink.util.Collector

import scala.collection.JavaConverters._

import br.ufscar.dc.ppgcc.gsdr.minas.Kdd.{dimension, magnitude, classes, magnitudeOne10th}

object KMeansVector extends App {
  val streamEnv = StreamExecutionEnvironment.getExecutionEnvironment
  val setEnv = ExecutionEnvironment.getExecutionEnvironment

  val kddPath = "./kdd.ics.uci.edu/databases/kddcup99/kddcup.data"
  val tempPath = "./tmpfs/kddcup.data"
  val filePathInput = if ( Files.exists(Paths.get(tempPath)) ) tempPath else kddPath
  val outFilePath = if ( Files.exists(Paths.get("tmpfs")) ) "./tmpfs/out" else "out"
  val relativeMagnitude: Int = magnitude / classes
  val relativeMagnitude10til: Int = magnitudeOne10th / classes
  val iterations = 10

  case class Point(value: Vector[Double]) extends IndexedSeq[Double] {
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
  case class Centroid(label: Double, value: Point, id: Int) {
    def matchLabel[A](other: Centroid, a: => A): A = if (this.label != other.label) throw new RuntimeException("Mismatch labels") else a
    def +(other: Centroid): Centroid = matchLabel(other, Centroid(this.label, value + other.value, this.id))
    def -(other: Centroid): Double = matchLabel(other, value - other.value)
    def *(scalar: Double): Centroid = Centroid(this.label, value * scalar, this.id)
    def /(scalar: Double): Centroid = this * (1/scalar)
    def unary_-  : Centroid = this * (-1)
  }
  def selectNearestCenter(centroids: IndexedSeq[Centroid])(point: Point): (Double, Centroid) =
    centroids.map(c => (point.euclideanDistance(c.value), c)).reduce(
      (a, b) => if (a._1 < b._1) a else b
    )

  //
  val trainingSet: DataSet[Centroid] = setEnv
    .readTextFile(filePathInput + "_10_percent")
    .map(x => {
      val kdd = Kdd.fromLine(x)
      Centroid(kdd(dimension), Point(kdd.take(dimension -1).toVector), kdd.hashCode())
    })
  val trainingSetPoints = trainingSet.map(_.value)

  val centroids = trainingSet
    .groupBy("label")
    .reduceGroup((values: Iterator[Centroid], out: Collector[Centroid]) =>
      values
        .grouped(relativeMagnitude10til /  10)
        .map(
          g => g.reduce((a, b) => a + b) / g.size
        )
      .foreach(out.collect)
    )
  val finalCentroids = centroids.iterate(iterations) { currentCentroids =>
    trainingSet
      .map(new RichMapFunction[Centroid, Centroid] {
        private var centroids: IndexedSeq[Centroid] = null
        override def open(parameters: Configuration): Unit = this.centroids = getRuntimeContext.getBroadcastVariable[Centroid]("centroids").asScala.toVector
        override def map(value: Centroid): Centroid = selectNearestCenter(centroids)(value.value)._2
      }).withBroadcastSet(currentCentroids, "centroids")
      .map { x => (x.id, x, 1L) }// .withForwardedFields("_1; _2")
      .groupBy(0)
      .reduce { (p1, p2) => (p1._1, p1._2 + p2._2, p1._3 + p2._3) } //.withForwardedFields("_1")
      .map { x => Centroid(id = x._1, value = (x._2 / x._3.toDouble).value, label = x._2.label) } //.withForwardedFields("id")
  }
  centroids.writeAsText(outFilePath + "/centroids", FileSystem.WriteMode.OVERWRITE)
  finalCentroids.writeAsText(outFilePath + "/centroids-final", FileSystem.WriteMode.OVERWRITE)
  setEnv.execute("base centroids")

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
