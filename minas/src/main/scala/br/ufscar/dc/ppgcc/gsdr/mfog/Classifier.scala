package br.ufscar.dc.ppgcc.gsdr.mfog

import java.io.File
import java.net.{InetAddress, Socket}
import java.time.LocalDateTime
import java.time.format.DateTimeFormatter

import br.ufscar.dc.ppgcc.gsdr.minas.kmeans.Point
import grizzled.slf4j.Logger
import org.apache.flink.api.common.functions.{MapFunction, RichMapFunction}
import org.apache.flink.api.scala.DataSet
import org.apache.flink.configuration.Configuration
import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment
import org.json.JSONObject

import scala.collection.mutable
import scala.io.BufferedSource

object Classifier {
  val LOG: Logger = Logger(getClass)
  def main(args: Array[String]): Unit = {
    val jobName = this.getClass.getName
    val dateString = LocalDateTime.now.format(DateTimeFormatter.ISO_DATE_TIME).replaceAll(":", "-")
    LOG.info(s"jobName = $jobName")
    val outDir = s"./out/$jobName/$dateString/"
    val dir = new File(outDir)
    if (!dir.exists) {
      if (!dir.mkdirs) throw new RuntimeException(s"Output directory '$outDir'could not be created.")
    }
    val env: StreamExecutionEnvironment = StreamExecutionEnvironment.getExecutionEnvironment

    //
    val modelSource = env.socketTextStream("localhost", 9997)
    modelSource.writeAsText(s"$outDir/classifier-model")
//    val modelStore = modelSource
//      .map[Seq[Cluster]](new RichMapFunction[String, Seq[Cluster]]() {
//        var model: mutable.Buffer[Cluster] = mutable.Buffer.empty
//        override def open(parameters: Configuration): Unit = {
//          super.open(parameters)
//          model = mutable.Buffer.empty
//        }
//        override def map(value: String): Seq[Cluster] = {
//          val cl = Cluster.fromJson(new JSONObject(value))
//          model.append(cl)
//          model
//        }
//      })
    //
    val testSrc = env.socketTextStream("localhost", 9996)
    testSrc.writeAsText(s"$outDir/classifier-test")
//    val testStream = testSrc
//    .map[Point](new MapFunction[String, Point]() {
//      override def map(value: String): Point = Point.fromJson(new JSONObject(value))
//    })
    //
    env.execute(jobName)
  }
}
