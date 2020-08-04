package br.ufscar.dc.ppgcc.gsdr.minas

import java.io.{File, FileWriter}
import java.time.LocalDateTime
import java.time.format.DateTimeFormatter
import java.util

import br.ufscar.dc.ppgcc.gsdr.mfog
import br.ufscar.dc.ppgcc.gsdr.mfog.{Cluster, Point}
import grizzled.slf4j.Logger
import br.ufscar.dc.ppgcc.gsdr.minas.kmeans._
import br.ufscar.dc.ppgcc.gsdr.utils.CollectionsUtils.RichIterator
import br.ufscar.dc.ppgcc.gsdr.utils.FlinkUtils.RichSet
import org.apache.flink.api.common.functions.{MapFunction, RichFilterFunction, RichMapFunction}
import org.apache.flink.api.common.typeinfo.{TypeInformation, _}
import org.apache.flink.api.java.functions.FunctionAnnotation.ForwardedFields
import org.apache.flink.api.java.operators.DataSink
import org.apache.flink.api.scala._
import org.apache.flink.configuration.Configuration
import org.apache.samoa.moa.cluster.{Clustering, SphereCluster, Cluster => MoaCluster}
import org.apache.samoa.moa.clusterers.{KMeans => MoaKMeans}

import scala.collection.{AbstractIterator, Iterator, immutable, mutable}
import scala.collection.JavaConverters._

object MinasFlinkOffline {
  val LOG: Logger = Logger(getClass)
  def main(args: Array[String]): Unit = {
    val jobName = this.getClass.getName
    val dateString = LocalDateTime.now.format(DateTimeFormatter.ISO_DATE_TIME).replaceAll(":", "-")
    LOG.info(s"jobName = $jobName")
    val setEnv = ExecutionEnvironment.getExecutionEnvironment

    val inPathIni = "datasets/kyoto-bin/kyoto_binario_binarized_offline_1class_fold1_ini"
    val inPathIndexed = s"$inPathIni.indexed"
    val outDir = s"./out/$jobName/$dateString/"
    val dir = new File(outDir)
    if (!dir.exists) {
      if (!dir.mkdirs) throw new RuntimeException(s"Output directory '$outDir'could not be created.")
    }
    val iterations = 10
    val k = 100
    val varianceThreshold = 1.0E-5

    indexInputFile(inPathIni, inPathIndexed)

    val training: DataSet[(String, Point)] = setEnv.readTextFile(inPathIndexed)
      .map(line => {
        // espera que a linha seja "int,double[],label"
        val lineSplit = line.split(",")
        val index = lineSplit.head.toLong
        val doubles = lineSplit.slice(1, lineSplit.length - 1).map(_.toDouble)
        val label = lineSplit.last
        (label, Point(index, doubles))
      })
    //
    LOG.info(s"training.parallelism = ${training.parallelism}")
    training.writeAsText(s"$outDir/initial")
    //
    serialKMeans(setEnv, outDir, k, training)
    serialClustream(setEnv, outDir, k, training)

//    parallelKmeans(k, outDir, iterations, varianceThreshold, training)

    setEnv.execute(jobName)
  }

  /*
  def parallelKmeans(k: Int, outDir: String, iterations: Int, varianceThreshold: Double, training: DataSet[(String, Point)]): DataSink[(String, Cluster)] = {
    val parallelKMeansInitial: DataSet[(String, Seq[Cluster], Seq[Point])] = training
      .groupBy(_._1)
      .reduceGroup(datapoints => {
        val dataSeq = datapoints.toSeq
        val label = dataSeq.head._1
        val points = dataSeq.map(_._2)
        val initial = points.take(k).map(p => Cluster(p.id, p, 0.0, label))
        (label, initial, points)
      })
    val initialClusters: DataSet[(String, Cluster)] = parallelKMeansInitial.flatMap(a => a._2.map(c => (c.label, c)))
    initialClusters.writeAsText(s"$outDir/parallelKMeans-initialClusters")
    //

    val fixedLoop = (0 to iterations).foldLeft(initialClusters)((prev, i) => {
      val next = iterationKMeansFlink(prev, training)
      next.writeAsText(s"$outDir/parallelKMeans-fixedLoop-$i")
      next
    })

    //    val fromFlinkExamples = initialClusters.iterate(iterations) { currentCentroids =>
    //      val newCentroids = training
    //        .withForwardedFields("*->_2")
    //        .map(new RichMapFunction[Point, (Long, Point)] {
    //          private var centroids: Traversable[Cluster] = _
    //
    //          /** Reads the centroid values from a broadcast variable into a collection. */
    //          override def open(parameters: Configuration): Unit = {
    //            centroids = getRuntimeContext.getBroadcastVariable[Cluster]("centroids").asScala
    //          }
    //
    //          override def map(p: Point): (Long, Point) = {
    //            var minDistance: Double = Double.MaxValue
    //            var closestCentroidId: Long = -1
    //            for (centroid <- centroids) {
    //              val distance = p.euclideanDistance(centroid.center)
    //              if (distance < minDistance) {
    //                minDistance = distance
    //                closestCentroidId = centroid.id
    //              }
    //            }
    //            (closestCentroidId, p)
    //          }
    //        })
    //        .withBroadcastSet(currentCentroids, "centroids")
    //        .map { x => (x._1, x._2, 1L) }.withForwardedFields("_1; _2")
    //        .groupBy(0)
    //        .reduce { (p1, p2) => (p1._1, p1._2.add(p2._2), p1._3 + p2._3) }.withForwardedFields("_1")
    //        .map { x => new Centroid(x._1, x._2.div(x._3)) }.withForwardedFields("_1->id")
    //      newCentroids
    //    }

    initialClusters
      .iterateWithTermination(iterations)(previous => {
        val previousVar = previous.groupBy(c => c._1)
          .reduceGroup(g => g.foldLeft(("", 0.0))((state, next) => (next._1, state._2 + next._2.variance)))
        val next = iterationKMeansFlink(previous, training)
        val term = next.groupBy(c => c._1)
          .reduceGroup(g => g.foldLeft(("", 0.0))((state, next) => (next._1, state._2 + next._2.variance)))
          .join(previousVar).where(0).equalTo(0)
          .map(v => (v._1._1, v._1._2 / v._2._2))
          .filter(v => v._2 < varianceThreshold)
        (next, term)
      })
      .writeAsText(s"$outDir/parallelKMeans-finalClusters")
  }

  def iterationKMeansFlink(clusters: DataSet[(String, Cluster)], points: DataSet[(String, Point)]): DataSet[(String, Cluster)] = {
    clusters
      .crossWithHuge(points)(
        (c, p) => if (p._1 != c._1) Seq.empty else Seq((p._1, p._2.id, c._2.id, p._2, c._2, p._2.distance(c._2.center)))
      )
      .flatMap(i => i)
      .groupBy(0, 1).min(5)
      .groupBy(1, 2).reduceGroup(g => {
      val sq = g.toSeq
      val h = sq.head
      (h._1, h._3, h._5, sq.map(i => (i._4, i._6)))
    })
      .map(group => {
        val label = group._1
        val c = group._3
        val nextCl = Cluster(c.id, Point.zero(c.center.dimension), 0.0, c.label)
        val next = group._4.foldLeft(nextCl)((state: Cluster, next: (Point, Double)) => {
          val p = next._1
          val d = next._2
          state.consumeWithDistance(p, d, group._4.size)
        })
        (label, next)
      })
  }
  */

  def serialKMeans: (ExecutionEnvironment, String, Int, DataSet[(String, Point)]) => DataSet[Cluster] = {
     def alg(points: Seq[Point], k: Int): Clustering = {
      val clusters: Array[MoaCluster] = points.take(k).map(
        p => new SphereCluster(p.value.toArray, Double.MaxValue).asInstanceOf[MoaCluster]
      ).toArray
      val dataPoints: util.List[MoaCluster] = points.map(
        p => new SphereCluster(p.value.toArray, 1.0).asInstanceOf[MoaCluster]
      ).toList.asJava
      MoaKMeans.kMeans(clusters, dataPoints)
    }
    moaClusterer("k-means", "serialKmeans", alg)
  }

  def serialClustream: (ExecutionEnvironment, String, Int, DataSet[(String, Point)]) => DataSet[Cluster] = {
    def algorith(points: Seq[Point], k: Int): Clustering = MoaKmeans.cluStream(points.map(_.value.toArray).toArray)
    moaClusterer("Clustream", "serialClustream", algorith)
  }

  def moaClusterer(name: String, fileName: String, algorith: (Seq[Point], Int) => Clustering)
                  (setEnv: ExecutionEnvironment, outDir: String, k: Int, training: DataSet[(String, Point)]): DataSet[Cluster] = {
    val clusters: DataSet[Cluster] = training
      .groupBy(_._1)
      .reduceGroup(dataPoints => {
        val dataSeq = dataPoints.toSeq
        val label = dataSeq.head._1
        val points: Seq[Point] = dataSeq.map(_._2)
        LOG.info(s"$name k=" + k)
        //
        val clustering: Clustering = algorith(points, k)
        //
        val clusteringVector = clustering.getClustering
        val clustersCenters: Seq[Cluster] = (0 to clusteringVector.size())
          .filter(i => clusteringVector.get(i) != null)
          .map(i => {
            val moaCluster: SphereCluster = clusteringVector.get(i).asInstanceOf[SphereCluster]
            val center: Array[Double] = moaCluster.getCenter
            val radius: Double = moaCluster.getRadius
            val id: Long = (if (moaCluster.getId > 0) moaCluster.getId else i).toLong
            val point: Point = Point(id, center)
            val cluster: Cluster = mfog.Cluster(point.id, point, radius, label, Cluster.CATEGORY_NORMAL, moaCluster.getWeight.toLong)
            cluster
          })
        val minDistances: Seq[(Point, Cluster, Double)] = points.map(p => {
          val d = clustersCenters.map(c => (c, p.distance(c.center))).minBy(_._2)
          (p, d._1, d._2)
        })
        val accounted = minDistances.groupBy(_._2.id).map(i => {
          val variance = i._2.maxBy(_._3)._3
          val count = i._2.size.toLong
          val cluster = i._2.head._2
          (cluster.id, variance, count)
        })
        val updatedClusters = clustersCenters.map(c =>
          accounted.find(i => i._1 == c.id).map(i => {
            val (id, variance, count) = i
            c.copy(variance=variance, matches=count)
          }).getOrElse(c)
        )
        (label, updatedClusters)
      })
      .flatMap(i => i._2)
    clusters.writeAsText(s"$outDir/$fileName")
    //
    setEnv.fromElements(Cluster.CSV_HEADER)
      .setParallelism(1)
      .union(clusters.map(c => c.csv).setParallelism(1))
      .map(cl => cl)
      .setParallelism(1)
      .writeAsText(s"$outDir/$fileName.csv")
    //
    setEnv.fromElements(Cluster.CSV_HEADER)
      .setParallelism(1)
      .union(
        clusters
          .filter(c => c.matches > 0 && c.variance > 0)
          .setParallelism(1)
          .filter(new RichFilterFunction[Cluster] {
            val seen: mutable.Set[Cluster] = mutable.Set[Cluster]()
            override def filter(value: Cluster): Boolean = {
              if (seen.contains(value)) {
                false
              } else {
                seen += value
                true
              }
            }
          })
          .map(c => c.csv)
          .setParallelism(1)
      )
      .map(cl => cl)
      .setParallelism(1)
      .writeAsText(s"$outDir/$fileName-clean.csv")
    //
    // val clustersClean = clusters.filter(cl => cl.)
    clusters
  }

  private def indexInputFile(inPathIni: String, inPathIndexed: String): Unit = {
    // adicionando uid as linhas
    val indexedFile = new File(inPathIndexed)
    if (!indexedFile.exists) {
      LOG.info("indexing input file.")
      val bufferedSource = io.Source.fromFile(inPathIni)
      val contents = bufferedSource.getLines.zipWithLongIndex
      indexedFile.createNewFile()
      val w = new FileWriter(indexedFile)
      contents.foreach(i => {
        val text = f"${i._2}%d,${i._1}%s%n" // String.format("%1$d,%2$s%n", i._2, i._1)
        w.write(text)
      })
      w.close()
      bufferedSource.close
    }
  }


}
