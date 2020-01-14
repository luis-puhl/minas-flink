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

object MinasKddSerialApp extends App {
  val LOG = Logger(getClass)
  val jobName = this.getClass.getSimpleName
  val time = System.currentTimeMillis()
  implicit val distanceOperator = Point.EuclideanSqrDistance
  LOG.info(s"jobName = $jobName, time=$time")
  LOG.info(s"Max distance = ${Point.max().fromOrigin}")

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
    val f = new File(path)
    Runtime.getRuntime.exec(s"mkdir -p ${f.getParent}")
    if (f.isFile) f.delete() else f.createNewFile()
    val writer = new PrintWriter(f)
    stream.foreach(s => writer.write(s"$s\n"))
    writer.close()
  }
  def afterConsumedHook(arg: (Option[String], Point, Cluster, Double)): Unit = {
    // LOG.info(s"afterConsumedHook $arg")
  }

  val training = getStreamFromFile(inPathIni)
  //
  val minas: Minas = Minas(config, afterConsumedHook, distanceOperator)
  val model: MinasModel = minas.offline(training)
  //
  val test = getStreamFromFile(inPathOnl)
  val initial = (model, Vector[(Option[String], Point)]() )
  val result: (MinasModel, Vector[(Option[String], Point)]) = test.foldLeft(initial)((acc, entry) => entry match {
    case (label, point) =>
      val (current, out) = acc
      val (result, next) = current.next(point)
      (next, out ++ result)
  })
  LOG.info(s"Ended MINAS in ${System.currentTimeMillis()} (${System.currentTimeMillis() - time}ms)")
  writeStreamToFile(result._2.map(i => i.toString), outFilePath + "/result")
  val evaluation = result._2.map(r => {
    val realLabel = test.find(p => p._2.id == r._2.id).get._1
    val matched = r._1 match {
      case Some(x) => x == realLabel
      case None => false
    }
    (realLabel, matched, r._2)
  })
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
