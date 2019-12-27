package br.ufscar.dc.ppgcc.gsdr.minas

import br.ufscar.dc.ppgcc.gsdr.minas.datasets.kdd._
import br.ufscar.dc.ppgcc.gsdr.minas.kmeans._
import br.ufscar.dc.ppgcc.gsdr.minas.kmeans.KMeansVector
import grizzled.slf4j.Logger
import org.apache.flink.api.scala._
import org.apache.flink.core.fs.FileSystem
import org.apache.flink.streaming.api.scala.{DataStream, StreamExecutionEnvironment}

object MinasKddCassales extends App {
  val LOG = Logger(getClass)
  val streamEnv = StreamExecutionEnvironment.getExecutionEnvironment
  val setEnv = ExecutionEnvironment.getExecutionEnvironment

  val inPathIni = "./tmpfs/KDDTe5Classes_fold1_ini.csv"
  val inPathOnl = "./tmpfs/KDDTe5Classes_fold1_onl.csv"
  val outFilePath = "./tmpfs/out"
  val iterations = 10

  val trainingSet: DataSet[KddCassalesEntry] = setEnv.readCsvFile[KddCassalesEntry](inPathIni)
  trainingSet.writeAsText(outFilePath + "/kdd-ini", FileSystem.WriteMode.OVERWRITE)

  val indexedTrainingSet: DataSet[(Long, KddCassalesEntry)] =
    org.apache.flink.api.scala.utils.DataSetUtils(trainingSet).zipWithUniqueId.rebalance()

//  val seedClusters = indexedTrainingSet
//    .groupBy(x => x._2.label)
//    .reduceGroup((all: Iterator[(Long, KddCassalesEntry)]) => {
//      val allVector: Vector[(Long, KddCassalesEntry)] = all.toVector
//      val label: String = allVector.head._2.label
//      val points: Seq[Point] = allVector.map(k => Point(k._2.value))
//      val seedPoints: Seq[Cluster] = KMeansVector.kmeansInitByFarthest(k = 100, points)
//      (label, seedPoints)
//    })
//  seedClusters.writeAsText(outFilePath + "/seedClusters", FileSystem.WriteMode.OVERWRITE)

  // val points = seedClusters.flatMap(x => x._2.map(p => (x._1, p)))
  val clusters: DataSet[(String, Seq[Cluster])] = indexedTrainingSet
    .groupBy(x => x._2.label)
    .reduceGroup((all: Iterator[(Long, KddCassalesEntry)]) => {
      val allVector: Vector[(Long, KddCassalesEntry)] = all.toVector
      val label: String = allVector.head._2.label
      val dataPoints = allVector.size
      LOG.info(s"Label '$label' contains $dataPoints data points.")
      val points: Seq[Point] = allVector.map(k => Point(k._2.value))
      // val seedPoints: Seq[Cluster] = KMeansVector.kmeansInitByFarthest(k = 100, points)
      //
      val iterations: Int = 10
      // val pointsDS: DataSet[Point] = setEnv.fromCollection(points)
      // val centroids: DataSet[Cluster] = setEnv.fromCollection(seedPoints)
      // val centroidsKmeans = KMeansVector.kmeansIteration(pointsDS, centroids, iterations).collect()
      // val centroidsKmeans = KMeansVector.kmeansIteration(points, seedPoints, iterations)
      val centroidsKmeans = KMeansVector.kmeans(label, 100, points)
      (label, centroidsKmeans)
    })
  clusters
    .writeAsText(outFilePath + "/clusters", FileSystem.WriteMode.OVERWRITE)

//  indexedTrainingSet.groupBy()
//  val clusters = KMeansVector.kmeansIteration()
//    seedClusters
//    // .rebalance()
//    // .groupBy(x => x._1)
//    // .reduceGroup()
//      KMeansVector.kmeansIteration(all, seedPoints, 10)
//      val initClusters: Iterable[Cluster] = points
//        // nearest
//        .map(p => seedPoints.map(c => (p.euclideanDistance(c._2), c, c._1, p)).minBy(x => x._1))
//        .groupBy(p => p._3)
//        .map(item => {
//          val center = item._2.head._2._2
//          val variance = item._2.map(s => s._4.euclideanDistance(center)).max
//          Cluster(item._1.toLong, label, center, variance)
//        })
//      val clusterDS = setEnv.fromCollection(initClusters)
//      kmeansIteration(points, clusterDS, iterations)
//    })
//    .writeAsText(outFilePath + "/kmeanspp-classified", FileSystem.WriteMode.OVERWRITE)

  setEnv.execute("base centroids")

//  val trainingStream: DataStream[KddCassalesEntry] = streamEnv
//    .readFile[KddCassalesEntry](KddCassalesEntry.inputFormat(inPathIni), filePath = inPathIni)
//  trainingStream.writeAsText(outFilePath + "/stream-kdd-ini", FileSystem.WriteMode.OVERWRITE)

}
