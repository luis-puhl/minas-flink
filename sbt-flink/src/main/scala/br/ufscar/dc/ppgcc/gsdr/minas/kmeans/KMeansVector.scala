package br.ufscar.dc.ppgcc.gsdr.minas.kmeans

import scala.util.{Random, Try}
import br.ufscar.dc.ppgcc.gsdr.minas.datasets.kdd.Kdd.{classes, magnitude, magnitudeOne10th}
import org.apache.flink.api.common.functions.{RichFilterFunction, RichMapFunction}
import org.apache.flink.api.scala._
import org.apache.flink.configuration.Configuration
import org.apache.flink.core.fs.FileSystem
import org.apache.flink.streaming.api.scala.StreamExecutionEnvironment
import grizzled.slf4j.Logger

import scala.collection.JavaConverters._
import scala.collection.{immutable, mutable}

object KMeansVector {
  val LOG = Logger(getClass)

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
          Cluster(old.id, newMeanPoint, newVariance)
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
            assert(seq.head._1 == cl.id, "Cluster id mismatch")
            val points = seq.map(x => x._3)
            val newCenter: Point = points.reduce((a, b) => a.add(b)).div(seq.size.toLong)
            val variance = points.map(p => newCenter.euclideanDistance(p)).max
            Cluster(cl.id, newCenter, variance)
          })
      c
    })
  }
  def kmeansIteration(points: Seq[Point], centroids: Seq[Cluster], iterations: Int): Seq[Cluster] = {
    val k = centroids.map(c => c.id).toSet.size
    var currentCentroids: Seq[Cluster] = centroids
    var currentVariance: Double = points
      .map(p => centroids.map(c => p.euclideanDistance(c.center)).min).sum
    LOG.info(s"k = $k")
    for {i <- 0 to iterations} {
      val a = points
        .map(p => currentCentroids.map(c => (c.id, c, p, p.euclideanDistance(c.center))).minBy(t => t._4))
      LOG.info(s"A size k = $k == ${a.size} ??")
      val b = a
        .groupBy(d => d._1)
      // assert(b.keySet.size == k, s"[Iter $i] Result centroids size is not k. Expected $k, got ${b.keySet.size}.")
      val c = b
        .values
        .map((sameCluster: Seq[(Long, Cluster, Point, Double)]) => {
          val cl = sameCluster.head._2
          assert(sameCluster.head._1 == cl.id, "Cluster id mismatch")
          val points = sameCluster.map(x => x._3)
          val newCenter: Point = points.reduce((a, b) => a.add(b)).div(sameCluster.size.toLong)
          val variance: Double = points.map(p => newCenter.euclideanDistance(p)).max
          Cluster(cl.id, newCenter, variance)
        })
        .toIndexedSeq
      // val variance = c.map(c => c.variance).sum
      currentCentroids = c
    }
    // assert(currentCentroids.size == k, s"Result centroids size is not k. Expected $k, got ${currentCentroids.size}.")
    currentCentroids
  }

  /**
   * Scala-esque tail-recursive implementation.
   *
   * @param points
   * @param centroids
   * @param iterations
   * @param varianceThreshold
   * @return
   */
  def kmeansIterationRec(points: Seq[Point], centroids: Seq[Cluster], iterations: Int, varianceThreshold: Double): (Seq[Cluster], Double) = {
    assert(varianceThreshold < 1, "[kmeansIterationRec] Variance threshold is in percent between iterations and must be less than 1.")
    val k = centroids.map(c => c.id).toSet.size
    LOG.info(s"[kmeansIterationRec] got ${points.size} points and $k clusters.")
    @scala.annotation.tailrec
    def iterate(currentCentroids: Seq[Cluster], prevVariance: Double, i: Int): (Seq[Cluster], Double) = {
      val nearest = points.map(
        p => currentCentroids
          .map(c => (c.id, c, p, p.euclideanDistance(c.center)))
          .minBy(t => t._4)
      )
      val uniqueClusters = nearest.map(n => n._1).toSet.size
      assert(uniqueClusters == k, s"[kmeansIterationRec] Expected k=$k, got $uniqueClusters on iteration $i.")
      val newCentroids = nearest
        .groupBy(d => d._1)
        .values
        .map((sameCluster: Seq[(Long, Cluster, Point, Double)]) => {
          val cl = sameCluster.head._2
          assert(sameCluster.head._1 == cl.id, "[kmeansIterationRec] Cluster id mismatch")
          val points = sameCluster.map(x => x._3)
          val newCenter: Point = points.reduce((a, b) => a.add(b)).div(sameCluster.size.toLong)
          val variance: Double = points.map(p => newCenter.euclideanDistance(p)).max
          Cluster(cl.id, newCenter, variance)
        })
        .toIndexedSeq
      val variance = newCentroids.map(c => c.variance).sum
      val varianceProgress = prevVariance / variance - 1.0
      LOG.info(s"[kmeansIterationRec] Iter $i, var $prevVariance -> $variance ($varianceProgress improvement, $varianceThreshold goal).")
      if (varianceThreshold > varianceProgress || i >= iterations) (newCentroids, variance)
      else iterate(newCentroids, variance, i + 1)
    }
    val currentVariance: Double = points
      .map(p => centroids.map(c => p.euclideanDistance(c.center)).min)
      .sum
    iterate(centroids, currentVariance, 1)
  }

  /**
   * Kmeans++ initialization algorith
   *
   * @param k
   * @param dataSet
   * @param seedFunction
   * @return
   */
  def kmeanspp(k: Int, dataSet: Seq[Point], seedFunction: () => Double = () => Math.random()): Seq[Cluster] = {
    @scala.annotation.tailrec
    def reduction(i: Int, points: Seq[Point], latest: Point, workSet: Seq[Cluster]): Seq[Cluster] = {
      if (i <= 0) workSet
      else {
        val next = points.maxBy(x => seedFunction() * x.euclideanDistance(latest))
        val remainingPoints = points.filter((p: Point) => next != p)
        reduction(i - 1, remainingPoints, next, workSet :+ Cluster(i, next, 0))
      }
    }
    val first = dataSet.head
    val workSet = IndexedSeq(Cluster(k, first, 0))
    val centers = reduction(k-1, dataSet.tail, first, workSet)
    val actualK = centers.map(c => c.id).toSet.size
    assert(actualK == k, s"[kmeanspp] Didn't get k = $k clusters. Got $actualK.")
    centers
  }

  /**
   * Best initializer for KDD :)
   * @param k
   * @param points
   * @return
   */
  def kmeansInitByFarthest(k: Int, points: Seq[Point]): Seq[Cluster] = {
    val npoints = points.size
    LOG.info(s"[kmeansInitByFarthest] got ${npoints} points and $k clusters.")
    assert(k < npoints, s"[kmeansInitByFarthest] Can't cluster $npoints points in k = $k.")
    @scala.annotation.tailrec
    def remaining(i: Int, points: Seq[Point], workSet: Seq[Cluster]): Seq[Cluster] = {
      LOG.info(s"[kmeansInitByFarthest] iter $i with ${workSet.size} clusters and ${points.size} points.")
      if (i >= k) workSet
      else {
        val farthest: (Double, Point) = points.map(x => (workSet.map(c => x.euclideanDistance(c.center)).max, x)).maxBy(p => p._1)
        LOG.info(s"[kmeansInitByFarthest] farthest ${farthest._2.id} with ${farthest._1}.")
        val remainingPoints = points.filter((p: Point) => farthest._2.id != p.id)
        val nextClusters = workSet :+ Cluster(farthest._2.id, farthest._2, 0)
        remaining(i + 1, remainingPoints, nextClusters)
      }
    }
    val workSet = IndexedSeq(Cluster(k, points.head, 0))
    val centers = remaining(1, points.tail, workSet)
    //
    val actualK = centers.map(c => c.id).toSet.size
    val actualKPoints = centers.map(c => c.center.id).toSet.size
    assert(actualK == k, s"[kmeansInitByFarthest] Didn't get k = $k clusters, got $actualK.")
    assert(actualK == k, s"[kmeansInitByFarthest] Didn't get k = $k clusters, got $actualKPoints K-Points.")
    val pointsWithNearest: Seq[(Long, Double, Cluster)] = points
      .map(p => {
        val closest = centers.map(c => (p.euclideanDistance(c.center), c)).minBy(d => d._1)
        (p.id, closest._1, closest._2)
      })
    val varianceCenters = pointsWithNearest
      .groupBy(i => i._3.id).values
      .map((sameCluster: Seq[(Long, Double, Cluster)]) => {
        val cluster = sameCluster.head._3
        val pointsCount = sameCluster.size
        val maxDistance = sameCluster.map(p => p._2).max
        (Cluster(cluster.id, cluster.center, maxDistance), pointsCount)
      })
    val count = varianceCenters.map(c => c._2)
    val vari = varianceCenters.map(c => c._1.variance)
    LOG.info(s"[kmeansInitByFarthest] Clusters with [${count.max}, ${count.min}] points with variance [${vari.max}, ${vari.min}]")
    varianceCenters.map(c => c._1).toSeq
  }
  /**
   * val distances = pointsTrainingSet.map(p => (1, p._2.fromOrigin))
   * val dSum = distances.reduce((a, b) => (a._1 + b._1, a._2 + b._2)).collect().head
   * val avg = dSum._2 / dSum._1
   * val variance = distances.map(p => math.pow(p._2 - avg, 2)).reduce(_+_)
   * .map(x => (dSum, avg, x, math.sqrt(x)))
   * .collect()
   * println(variance)
   * // Buffer(((48791, 97411.4263331826), 1.9965039932197044, 7106.52509237684, 84.30020813958195))
   *            count, sum,                 avg                 var,              std-dev
   */
  def byZeroDistance(k: Int, dataSet: Seq[Point]): Seq[Cluster] = {
    assert(dataSet.size > k, s"Dataset contains less than k. Expected $k, got ${dataSet.size}")
    val centers: Seq[(Point, Double)] = dataSet
      .map(p => (p, p.fromOrigin))
      .sortBy(p => p._2)
      .grouped(dataSet.size / k)
      // .sliding(dataSet.size / k)
      .map(slice => slice(Random.nextInt(slice.size)))
      .take(k)
      .toIndexedSeq
    val actualKCenters = centers.map(c => c._1.id).toSet.size
    assert(actualKCenters == k, s"Didn't get k = $k clusters. Got $actualKCenters.")
    val clusters = dataSet
      .map(p => centers
        .map(c => (c._1.id, c._1, c._1.euclideanDistance(p)))
        .minBy(d => d._3)
      )
      .groupBy(d => d._1).values
      .map(dset =>
        dset.reduce((a, b) => (a._1, a._2, a._3.max(b._3))) match {
          case (id: Long, p: Point, d: Double) => Cluster(id, p, d)
        })
      .toIndexedSeq
    val actualK = clusters.map(c => c.id).toSet.size
    assert(actualK == k, s"Didn't get k = $k clusters. Got $actualK.")
    clusters
  }
  //
  def kmeans(labelName: String, k: Int, points: Seq[Point], iterations: Int, varianceThreshold: Double): (Seq[Cluster], Double) = {
    def initialization(methodName: String, method: => Seq[Cluster]): (String, (Seq[Cluster], Double)) = {
      LOG.info(s"[$labelName] Running $methodName.")
      try {
        (methodName, kmeansIterationRec(points, method, iterations, varianceThreshold))
      } catch {
        case e: AssertionError => {
          LOG.info(e.getMessage)
          throw e
        }
        case e: Exception => {
          LOG.info(e.getMessage)
          (s"$methodName (${e.getMessage})", (Seq[Cluster](), Double.MaxValue))
        }
      }
    }

    val tries = mutable.IndexedSeq[(String, (Seq[Cluster], Double))](
      initialization("kmeansInitByFarthest", kmeansInitByFarthest(k, points)),
      initialization("kmeanspp [1]", kmeanspp(k, points)),
      initialization("kmeanspp [2]", kmeanspp(k, points)),
      initialization("kmeanspp [3]", kmeanspp(k, points))
      // initialization("byZeroDistance [1]", byZeroDistance(k, points)),
      // initialization("byZeroDistance [2]", byZeroDistance(k, points)),
      // initialization("byZeroDistance [3]", byZeroDistance(k, points))
      )
    val best = tries.minBy(t => t._2._2)
    LOG.info(s"[$labelName] Got best result with ${best._1} algorithm with ${best._2._2} variance.")
    best._2
  }
}
