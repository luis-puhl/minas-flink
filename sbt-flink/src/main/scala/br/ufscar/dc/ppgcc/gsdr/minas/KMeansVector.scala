package br.ufscar.dc.ppgcc.gsdr.minas

import java.nio.file.{Files, Paths}
import java.util

import org.apache.flink.api.common.functions.{RichFilterFunction, RichMapFunction}
import org.apache.flink.api.scala._
import org.apache.flink.configuration.Configuration
import org.apache.flink.core.fs.FileSystem
import org.apache.flink.streaming.api.scala.{DataStream, StreamExecutionEnvironment}
import org.apache.flink.util.Collector

import scala.collection.JavaConverters._
import br.ufscar.dc.ppgcc.gsdr.minas.datasets.kdd.Kdd.{classes, dimension, magnitude, magnitudeOne10th}
import br.ufscar.dc.ppgcc.gsdr.minas.datasets.kdd.KddCassalesEntry
import br.ufscar.dc.ppgcc.gsdr.minas.kmeans.{Cluster, Point}
import org.apache.flink.api.common.operators.Order

import scala.collection.{immutable, mutable}

object KMeansVector {
  val streamEnv = StreamExecutionEnvironment.getExecutionEnvironment
  val setEnv = ExecutionEnvironment.getExecutionEnvironment

  val inPathIni = "./tmpfs/KDDTe5Classes_fold1_ini.csv"
  val inPathOnl = "./tmpfs/KDDTe5Classes_fold1_onl.csv"
  val outFilePath = "./tmpfs/out"
  val relativeMagnitude: Int = magnitude / classes
  val relativeMagnitude10til: Int = magnitudeOne10th / classes
  val iterations = 10




  def kmeansIteration(points: DataSet[Point], centroids: DataSet[Cluster], iterations: Int): DataSet[Cluster] = {
    centroids.iterate(iterations)(currentCentroids =>
      points
        .map(new RichMapFunction[Point, (Long, Cluster, mutable.IndexedSeq[Point], Long)] {
          private var centroids: Traversable[Cluster] = null

          override def open(parameters: Configuration): Unit = {
            this.centroids = getRuntimeContext.getBroadcastVariable[Cluster]("centroids").asScala
          }

          def map(p: Point): (Long, Cluster, mutable.IndexedSeq[Point], Long) = {
            val minDistance: (Double, Cluster) = centroids.map(c => (p.euclideanDistance(c.center), c)).minBy(_._1)
            (minDistance._2.id, minDistance._2, mutable.IndexedSeq(p), 1L)
          }
        })
        .withBroadcastSet(currentCentroids, "centroids")
        // .withForwardedFields("_1; _2")
        .groupBy(0)
        .reduce( (p1, p2) => (p1._1, p1._2, p1._3.++(p2._3), p1._4 + p2._4) )
        // .withForwardedFields("_1")
        .map(x => {
          val old = x._2
          val newMeanPoint = x._3.reduce((a, b) => a.add(b)).div(x._4)
          val newVariance = x._3.map(p => p.euclideanDistance(newMeanPoint)).max
          Cluster(old.id, old.label, newMeanPoint, newVariance)
        })
        // .withForwardedFields("_1->id")
    )
  }
  def kmeansIteration(points: Seq[Point], centroids: DataSet[Cluster], iterations: Int): DataSet[Cluster] = {
    centroids.iterate(iterations)((currentCentroids: DataSet[Cluster]) => {
      val a: DataSet[(Cluster, Point, Double)] = currentCentroids
        .flatMap(c => points.map(p => (c, p, c.center.euclideanDistance(p))))
      val b: DataSet[(Long, Cluster, Point, Double)] = a.groupBy(d => d._2.hashCode())
          .reduceGroup(samePoint => {
            val seq = samePoint.toIndexedSeq
            val p: Point = seq.head._2
            val nearest: (Cluster, Point, Double) = seq.minBy(p => p._3)
            (nearest._1.id, nearest._1, p, nearest._3)
          })
      val c: DataSet[Cluster] = b.groupBy(0)
          .reduceGroup(sameCluster => {
            val seq = sameCluster.toIndexedSeq
            val cl = seq.head._2
            assert(seq.head._1 == cl.id)
            val points = seq.map(x => x._3)
            val newCenter: Point = points.reduce((a, b) => a.add(b)).div(seq.size.toLong)
            val variance = points.map(p => newCenter.euclideanDistance(p)).max
            Cluster(cl.id, cl.label, newCenter, variance)
          })
      c
    })
  }

  def kmeanspp(k: Int, dataSet: DataSet[Point], tries: Int): DataSet[Point] = {
    // val count = dataSet.count()
    val f: Point = dataSet.first(1).collect().head
    val seed: DataSet[(Double, Point)] = dataSet.first(1)
      .map((x: Point) => (x.euclideanDistance(f), x))
    seed.iterate(k)((solution: DataSet[(Double, Point)]) => dataSet
        .filter(new RichFilterFunction[Point] {
          var chosenOnes: Traversable[(Double, Point)] = null
          override def open(parameters: Configuration): Unit = {
            chosenOnes = getRuntimeContext.getBroadcastVariable[(Double, Point)]("chosenOnes").asScala
            super.open(parameters)
          }
          override def filter(value: Point): Boolean = !chosenOnes.exists(p => p._2 == value)
        }).withBroadcastSet(solution, "chosenOnes")
      .map((x: Point) => (Math.random() * x.euclideanDistance(f), x))
      .max(0).first(1)
      .union(solution)
      ).map(x => x._2)
  }
  def kmeanspp(k: Int, dataSet: Seq[Point], tries: Int): Seq[(Int, Point)] = {
    var current = dataSet.head
    val seed: Seq[(Int, Point)] = mutable.IndexedSeq((0, current))
    for (i <- 0 to k) {
      current = dataSet
        .filter((p: Point) => !seed.exists(c => c._2 == p))
        .map((x: Point) => (Math.random() * x.euclideanDistance(current), x))
        .maxBy(x => x._1)._2
      seed.+:((i, current))
    }
    seed
  }

  val trainingSet: DataSet[KddCassalesEntry] = setEnv.readCsvFile[KddCassalesEntry](inPathIni)
  trainingSet.writeAsText(outFilePath + "/kdd-ini", FileSystem.WriteMode.OVERWRITE)

  val indexedTrainingSet: DataSet[(Long, KddCassalesEntry)] =
    org.apache.flink.api.scala.utils.DataSetUtils(trainingSet).zipWithUniqueId.rebalance()

  kmeanspp(100, indexedTrainingSet.map(k => Point(k._2.value)), 2)
    .writeAsText(outFilePath + "/kmeanspp", FileSystem.WriteMode.OVERWRITE)

  indexedTrainingSet
    .groupBy(x => x._2.label)
    .reduceGroup(all => {
      val allVector: Vector[(Long, KddCassalesEntry)] = all.toVector
      val label: String = allVector.head._2.label
      val points: Seq[Point] = allVector.map(k => Point(k._2.value))
      val seedPoints: Seq[(Int, Point)] = kmeanspp(100, points, 2)
      val initClusters: Iterable[Cluster] = points
        // nearest
        .map(p => seedPoints.map(c => (p.euclideanDistance(c._2), c, c._1, p)).minBy(x => x._1))
        .groupBy(p => p._3)
        .map(item => {
          val center = item._2.head._2._2
          val variance = item._2.map(s => s._4.euclideanDistance(center)).max
          Cluster(item._1.toLong, label, center, variance)
        })
      val clusterDS = setEnv.fromCollection(initClusters)
      kmeansIteration(points, clusterDS, iterations)
    })
    .writeAsText(outFilePath + "/kmeanspp-classified", FileSystem.WriteMode.OVERWRITE)

//  val centroids1 = indexedTrainingSet
//    .map(k => {
//      val c = Cluster(k._1, k._2.label, Point(k._2.value), 0.0)
//      (c.center.fromOrigin, c)
//    })
//    .sortPartition(0, Order.ASCENDING)
//  centroids1.writeAsText(outFilePath + "/sorted", FileSystem.WriteMode.OVERWRITE)
//
//  centroids1
//    .map(_._2)
//    .groupBy("label").reduceGroup((vs, out: Collector[Cluster]) => {
//      val list = vs.toList
//      out.collect(list.head)
//      out.collect(list.last)
//    })
//    .writeAsText(outFilePath + "/head-tail", FileSystem.WriteMode.OVERWRITE)
//  val centroids = centroids1
//    .map(_._2)
//    .groupBy("label")
//    .reduceGroup( (values: Iterator[Cluster], out: Collector[(String, List[Double])]) =>
//      values.map(_.label).toSet[String].toList
//        .map(x => (x, values.filter(p => p.label == x).map(b => b.center.fromOrigin).toList ))
//        .foreach(out.collect)
//    )
//  centroids.writeAsText(outFilePath + "/distribution", FileSystem.WriteMode.OVERWRITE)
//
////  val finalCentroidsLabeled = centroids.iterate(iterations)(currentCentroids =>
////    trainingSet.map(f => (f.label, f))
////      .map(new RichMapFunction[(Double, Centroid), (Int, Centroid, Long)] {
////        private var centroids: IndexedSeq[Centroid] = null
////        override def open(parameters: Configuration): Unit =
////          this.centroids = getRuntimeContext.getBroadcastVariable[Centroid]("centroids").asScala.toVector
////        override def map(value: (Double, Centroid)): (Int, Centroid, Long) = {
////          (value._2.id, selectNearestCenter(centroids.filter(c => c.label == value._1))(value._2.value)._2, 1L)
////        }
////      }).withBroadcastSet(currentCentroids, "centroids")
////      .groupBy(0)
////      .reduce { (p1, p2) => (p1._1, p1._2 + p2._2, p1._3 + p2._3) } //.withForwardedFields("_1")
////      .map { x => Centroid(id = x._1, value = (x._2 / x._3.toDouble).value, label = x._2.label) } //.withForwardedFields("id")
////  )
////  finalCentroidsLabeled.writeAsText(outFilePath + "/centroids-labeled", FileSystem.WriteMode.OVERWRITE)
//
////  val finalCentroids = centroids.iterate(iterations) { currentCentroids =>
////    trainingSet
////      .map(new RichMapFunction[Centroid, Centroid] {
////        private var centroids: IndexedSeq[Centroid] = null
////        override def open(parameters: Configuration): Unit = this.centroids = getRuntimeContext.getBroadcastVariable[Centroid]("centroids").asScala.toVector
////        override def map(value: Centroid): Centroid = selectNearestCenter(centroids)(value.value)._2
////      }).withBroadcastSet(currentCentroids, "centroids")
////      .map { x => (x.id, x, 1L) }// .withForwardedFields("_1; _2")
////      .groupBy(0)
////      .reduce { (p1, p2) => (p1._1, p1._2 + p2._2, p1._3 + p2._3) } //.withForwardedFields("_1")
////      .map { x => Centroid(id = x._1, value = (x._2 / x._3.toDouble).value, label = x._2.label) } //.withForwardedFields("id")
////  }
////  finalCentroids.writeAsText(outFilePath + "/centroids-final", FileSystem.WriteMode.OVERWRITE)

  setEnv.execute("base centroids")
}
