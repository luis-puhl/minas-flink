package br.ufscar.dc.ppgcc.gsdr.minas.kmeans

import br.ufscar.dc.ppgcc.gsdr.minas.datasets.kdd.Kdd.{classes, magnitude, magnitudeOne10th}
import org.apache.flink.api.common.functions.{RichFilterFunction, RichMapFunction}
import org.apache.flink.api.scala._
import org.apache.flink.configuration.Configuration
import org.apache.flink.core.fs.FileSystem
import org.apache.flink.streaming.api.scala.StreamExecutionEnvironment

import scala.collection.JavaConverters._
import scala.collection.mutable

object KMeansVector {
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
}
