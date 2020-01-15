package br.ufscar.dc.ppgcc.gsdr.minas

import java.io.{File, PrintWriter}

import br.ufscar.dc.ppgcc.gsdr.minas.datasets.kdd._
import br.ufscar.dc.ppgcc.gsdr.minas.kmeans._
import grizzled.slf4j.Logger
import org.apache.flink.api.scala.ExecutionEnvironment
import org.apache.flink.core.fs.FileSystem
import org.apache.flink.streaming.api.scala._
import org.apache.flink.streaming.api.windowing.assigners.TumblingProcessingTimeWindows
import org.apache.flink.streaming.api.windowing.time.Time

import scala.collection.immutable
import java.util.Properties

object MinasKddSerialApp {
  val LOG = Logger(getClass)
  val jobName = this.getClass.getSimpleName
  val time = System.currentTimeMillis()
  implicit val distanceOperator = Point.EuclideanSqrDistance

  val inPathIni = "./tmpfs/KDDTe5Classes_fold1_ini.csv"
  val inPathOnl = "./tmpfs/KDDTe5Classes_fold1_onl.csv"
  val outFilePath = "./tmpfs/out/MinasKddSerialApp"
  val iterations = 10
  val k = 100
  val varianceThreshold = 1.0E-5

  val config: Map[String, Int] = Map(
    "k" -> k,
    "noveltyDetectionThreshold" -> 1000,
    "representativeThreshold" -> 10,
    "noveltyIndex" -> 0
  )

  def getStreamFromFile(path: String): Vector[(String, Point)] = {
    LOG.info(s"reading $path")
    val bufferedSource = io.Source.fromFile(path)
    val stream: Vector[(String, Point)] = bufferedSource.getLines
      .map(line => KddCassalesEntryFactory.fromStringLine(line))
      .zipWithIndex
      .map(entry => (entry._1.label, Point(entry._2, entry._1.value)))
      .toVector
    bufferedSource.close
    stream
  }
  def writeStreamToFile(stream: Seq[String], path: String): Unit = {
    LOG.info(s"writing $path")
    val f = new File(path)
    Runtime.getRuntime.exec(s"mkdir -p ${f.getParent}")
    if (f.isFile) f.delete() else f.createNewFile()
    val writer = new PrintWriter(f)
    stream.foreach(s => writer.write(s"$s\n"))
    writer.close()
  }

  def main(args: Array[String]): Unit = {
//    val properties = new Properties
//    properties.load()
    println(this.getClass.getResource("app.properties"))
//    LOG.info(s"properties = $properties")
    LOG.info(s"jobName = $jobName, time=$time")
    LOG.info(s"Max distance = ${Point.max().fromOrigin}")
    //
    val training = getStreamFromFile(inPathIni)
    //
    val minas: Minas = Minas(config, distanceOperator)
    LOG.info(s"minas.offline(training))")
    val model: MinasModel = minas.offline(training)
    LOG.info(s"minas.offline(training)) Done")
    writeStreamToFile(model.model.map(_.toString), outFilePath + "/offline_training")
    //
    val test: Seq[(String, Point)] = getStreamFromFile(inPathOnl)
    val result: (Seq[(Option[String], Point)], MinasModel) = minas.online(model, test.map(_._2))
    LOG.info(s"Ended MINAS in ${System.currentTimeMillis()} (${System.currentTimeMillis() - time}ms)")
    //
    writeStreamToFile(result._1.map(i => i.toString), outFilePath + "/result")
    LOG.info(s"evaluation of ${test.size} entries")
    val realIdLabelMap: Map[Long, String] = test.groupBy(_._2.id).mapValues(_.head._1)
    val resultIdLabelMap: Map[Long, (String, Boolean, Seq[String], Point)] = result._1.groupBy(_._2.id).mapValues(allEvaluations => {
      val point = allEvaluations.head._2
      val id = point.id
      val realLabel = realIdLabelMap(id)
      if (id % 2000 == 0) {
        LOG.info(s"progress ${id} / ${test.size}\n")
      }
      val matched = allEvaluations.exists(ev => ev._1 match {
        case Some(x) => x == realLabel
        case None => false
      })
      (realLabel, matched, allEvaluations.map(_._1.getOrElse("_unknown_")), point)
    })
    val evaluation = resultIdLabelMap.toVector
    writeStreamToFile(evaluation.map(i => i.toString), outFilePath + "/evaluation")
    /*
      val streamEnv = StreamExecutionEnvironment.getExecutionEnvironment
      // val setEnv = ExecutionEnvironment.getExecutionEnvironment
      val resultStream = streamEnv.fromCollection(result._2)
      resultStream.writeAsText(outFilePath + "/result", FileSystem.WriteMode.OVERWRITE)
      //
      val testStream = streamEnv.fromCollection(test)
      resultStream
        .join(testStream).where(r => r._2.id).equalTo(t => t._2.id)
        .window(TumblingProcessingTimeWindows.of(Time.minutes(10)))
        .apply((res, tes) => {
          val didMatch = res._1 match {
            case Some(x) => x == tes._1
            case None => false
          }
          (tes._1, didMatch, tes._2)
        })
        .writeAsText(outFilePath + "/evaluation", FileSystem.WriteMode.OVERWRITE)
    //    .keyBy(_._2.id)
    //    .map(i => {
    //      val (computeLabel, point) = i
    //      val (trueLabel, truePoint) = test.find(t => t._2.id == point.id).get
    //      val didMatch = computeLabel match {
    //        case Some(x) => x == trueLabel
    //        case None => false
    //      }
    //      (trueLabel, didMatch, truePoint)
    //    })


      val executed = streamEnv.execute(jobName)

     */
    val endTime = System.currentTimeMillis()
    LOG.info(s"Ended App in $endTime (${endTime - time}ms)")
  }
}
