package br.ufscar.dc.ppgcc.gsdr.minas

import br.ufscar.dc.ppgcc.gsdr.minas.datasets.kdd._
import br.ufscar.dc.ppgcc.gsdr.minas.kmeans._
import br.ufscar.dc.ppgcc.gsdr.minas.kmeans.KMeansVector.{kmeansIteration, kmeanspp}

import org.apache.flink.api.scala._
import org.apache.flink.core.fs.FileSystem
import org.apache.flink.streaming.api.scala.StreamExecutionEnvironment

object MinasKddCassales extends App {
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

  setEnv.execute("base centroids")
}
