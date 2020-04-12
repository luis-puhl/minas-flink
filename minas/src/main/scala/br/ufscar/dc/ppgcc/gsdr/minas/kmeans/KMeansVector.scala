package br.ufscar.dc.ppgcc.gsdr.minas.kmeans

import br.ufscar.dc.ppgcc.gsdr.mfog
import br.ufscar.dc.ppgcc.gsdr.mfog.Cluster

import scala.util.{Random, Try}
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
          val newMeanPoint = x._3.reduce((a, b) => a.+(b))./(x._4)
          val newVariance = x._3.map(p => p.euclideanDistance(newMeanPoint)).max
          mfog.Cluster(old.id, newMeanPoint, newVariance, old.label)
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
            val newCenter: Point = points.reduce((a, b) => a.+(b))./(seq.size.toLong)
            val variance = points.map(p => newCenter.euclideanDistance(p)).max
            mfog.Cluster(cl.id, newCenter, variance, cl.label)
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
          val newCenter: Point = points.reduce((a, b) => a.+(b))./(sameCluster.size.toLong)
          val variance: Double = points.map(p => newCenter.euclideanDistance(p)).max
          mfog.Cluster(cl.id, newCenter, variance, cl.label)
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
        .groupBy(d => d._1).values
        .map((sameCluster: Seq[(Long, Cluster, Point, Double)]) => {
          val cl = sameCluster.head._2
          assert(sameCluster.head._1 == cl.id, "[kmeansIterationRec] Cluster id mismatch")
          val points = sameCluster.map(x => x._3)
          val newCenter: Point = points.reduce((a, b) => a.+(b))./(sameCluster.size.toLong)
          val variance: Double = points.map(p => newCenter.euclideanDistance(p)).max
          mfog.Cluster(cl.id, newCenter, variance, cl.label)
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
  def kmeanspp(label: String, k: Int, dataSet: Seq[Point], seedFunction: () => Double = () => Math.random()): Seq[Cluster] = {
    @scala.annotation.tailrec
    def reduction(i: Int, points: Seq[Point], latest: Point, workSet: Seq[Cluster]): Seq[Cluster] = {
      if (i <= 0) workSet
      else {
        val next = points.maxBy(x => seedFunction() * x.euclideanDistance(latest))
        val remainingPoints = points.filter((p: Point) => next != p)
        reduction(i - 1, remainingPoints, next, workSet :+ mfog.Cluster(i, next, 0, label))
      }
    }
    val first = dataSet.head
    val workSet = IndexedSeq(mfog.Cluster(k, first, 0, label))
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
  def kmeansInitByFarthest(k: Int, points: Seq[Point], iterLimit: Int, label: String): Seq[Cluster] = {
    val npoints = points.size
    LOG.info(s"[$label] [kmeansInitByFarthest] got ${npoints} points and $k clusters.")
    assert(k < npoints, s"[$label] [kmeansInitByFarthest] Can't cluster $npoints points in k = $k.")
    @scala.annotation.tailrec
    def remaining(i: Int, points: Seq[Point], workSet: Seq[Cluster], iterLimit: Int): Seq[Cluster] = {
      if (iterLimit <= 0) throw new RuntimeException(s"[$label] [kmeansInitByFarthest] Exceeded iterations. i=$i, p=${points.size}.")
      if (i >= k) workSet
      else {
        val xDistance: Seq[(Point, Cluster, Double)] = crossDistance(points, workSet)
        val byPoint: Iterable[Seq[(Point, Cluster, Double)]] = xDistance.groupBy(d => d._1.id).values
        val farthest: (Point, Cluster, Double) = byPoint.map(d => d.minBy(d2 => d2._3)).maxBy(d => d._3)
        LOG.info(s"[$label] i=$i farthest = (p= ${farthest._1.id}, c= ${farthest._2.id}, d= ${farthest._3}), rem= ${workSet.size}")
        val farthestPoint: Point = farthest._1
        val nextClusters = workSet :+ mfog.Cluster(farthestPoint.id, farthestPoint, 0, label)
        remaining(nextClusters.size, points, nextClusters, iterLimit - 1)
      }
    }
    val workSet = IndexedSeq(mfog.Cluster(points.head.id, points.head, 0, label))
    val centers = remaining(1, points.tail, workSet, iterLimit)
    //
    val xDistance: Seq[(Point, Cluster, Double)] = crossDistance(points, centers)
    val byPoint: Iterable[Seq[(Point, Cluster, Double)]] = xDistance.groupBy(d => d._1.id).values
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
        (Cluster(cluster.id, cluster.center, maxDistance, cluster.label), pointsCount)
      })
    val count = varianceCenters.map(c => c._2)
    val vari = varianceCenters.map(c => c._1.variance)
    val finalClusters = varianceCenters.map(c => c._1).toSeq
    val actualKPoints = finalClusters.map(c => c.center.id).toSet.size
    LOG.info(s"[$label] [kmeansInitByFarthest] ${finalClusters.size} Clusters with [${count.min}..${count.max}] points with variance [${vari.min}..${vari.min}]")
    assert(actualKPoints == k, s"[$label] [kmeansInitByFarthest] Didn't get k = $k clusters, got $actualKPoints K-Points.")
    finalClusters
  }

  def crossDistance(points: Seq[Point], clusters: Seq[Cluster]): Seq[(Point, Cluster, Double)] =
    points.flatMap(p => clusters.map(c => (p, c, p.euclideanDistance(c.center))))

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
      initialization("kmeansInitByFarthest", kmeansInitByFarthest(k, points, (1.4*k).toInt, labelName)),
      initialization("kmeanspp [1]", kmeanspp(labelName, k, points)),
      initialization("kmeanspp [2]", kmeanspp(labelName, k, points)),
      initialization("kmeanspp [3]", kmeanspp(labelName, k, points))
      // initialization("byZeroDistance [1]", byZeroDistance(k, points)),
      // initialization("byZeroDistance [2]", byZeroDistance(k, points)),
      // initialization("byZeroDistance [3]", byZeroDistance(k, points))
      )
    val best = tries.minBy(t => t._2._2)
    LOG.info(s"[$labelName] Got best result with ${best._1} algorithm with ${best._2._2} variance.")
    best._2
  }
}
