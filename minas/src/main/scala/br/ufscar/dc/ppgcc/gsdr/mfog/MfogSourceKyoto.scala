package br.ufscar.dc.ppgcc.gsdr.mfog

import java.io.PrintStream
import java.net.{InetAddress, ServerSocket, Socket}
import java.time.LocalDateTime
import java.time.format.DateTimeFormatter

import br.ufscar.dc.ppgcc.gsdr.minas.kmeans.{MfogCluster, Point}
import br.ufscar.dc.ppgcc.gsdr.utils.CollectionsUtils.RichIterator
import br.ufscar.dc.ppgcc.gsdr.utils.FlinkUtils.RichSet
import grizzled.slf4j.Logger
import org.apache.flink.api.common.functions.RichMapFunction
import org.apache.flink.api.scala.ExecutionEnvironment
import org.apache.flink.api.scala._
import org.apache.flink.configuration.Configuration
import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment

import scala.io.BufferedSource

object MfogSourceKyoto {
  val LOG: Logger = Logger(getClass)

  def main(args: Array[String]): Unit = {
    val jobName = this.getClass.getName
    val dateString = LocalDateTime.now.format(DateTimeFormatter.ISO_DATE_TIME).replaceAll(":", "-")
    /*
    val outDir = s"./out/$jobName/$dateString/"
    val dir = new File(outDir)
    if (!dir.exists) {
      if (!dir.mkdirs) throw new RuntimeException(s"Output directory '$outDir'could not be created.")
    }
     */
    LOG.info(s"jobName = $jobName")
    val setEnv: ExecutionEnvironment = ExecutionEnvironment.getExecutionEnvironment
    val env: StreamExecutionEnvironment = StreamExecutionEnvironment.getExecutionEnvironment
    //
    val trainingSet: Seq[(String, Point)] = trainingData(setEnv)
    //
    val server = new ServerSocket(9999)
    LOG.info("server ready")
    while (true) {
      val s = server.accept()
      LOG.info("connected")
      // val in = new BufferedSource(s.getInputStream).getLines()
      val out = new PrintStream(s.getOutputStream)
      def toMsg(x: (String, Point)): String = {
        val (l, p) = x
        s"$l>${p.csv}"
      }
      LOG.info(s"sending ${toMsg(trainingSet.head)}")
      trainingSet.foreach(x => out.println(toMsg(x)))
      out.flush()
      s.close()
      LOG.info(s"done sending ${trainingSet.size}")
    }
  }

  def trainingData(setEnv: ExecutionEnvironment): Seq[(String, Point)] = {
    val testPath = "datasets/kyoto-bin/kyoto_binario_binarized_offline_1class_fold1_ini"
    setEnv.readTextFile(testPath).map[(String, Point)](new MapToMinasPoints()).collect()
  }

  def modelData(setEnv: ExecutionEnvironment): Seq[MfogCluster] = {
    val modelPath = "datasets/models/offline-clean.csv"
    /*
    Int,String,String,Long,Long,Double,Double,Double[]
    id,label,category,matches,time,meanDistance,radius,center
    0,N,normal,502,0,0.04553028494064095,0.1736759823342961,[2.888834262948207E-4, 0.020268260292164667, 0.04161011127902189, 0.020916334661354643, 1.0, 0.0, 0.0026693227091633474, 0.516593625498008, 0.5267529880478092, 1.9920318725099602E-4, 0.0, 7.968127490039841E-5, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0]
    */
    val clustersSet = setEnv.readTextFile(modelPath).map[MfogCluster](new MapCsvToMfogCluster())
    clustersSet.collect()
  }

  def testData(setEnv: ExecutionEnvironment): Seq[(String, Point)] = {
    val testPath = "datasets/kyoto-bin/kyoto_binario_binarized_offline_1class_fold1_onl"
    setEnv.readTextFile(testPath).map[(String, Point)](new MapToMinasPoints()).collect()
  }

  def simpleServer() = {
    val server = new ServerSocket(9999)
    while (true) {
      val s = server.accept()
      val in = new BufferedSource(s.getInputStream).getLines()
      val out = new PrintStream(s.getOutputStream)

      out.println(in.next())
      out.flush()
      s.close()
    }
  }
  def simpleClient() = {
    val s = new Socket(InetAddress.getByName("localhost"), 9999)
    lazy val in = new BufferedSource(s.getInputStream).getLines()
    val out = new PrintStream(s.getOutputStream)

    out.println("Hello, world")
    out.flush()
    println("Received: " + in.next())

    s.close()
  }

  class MapToMinasPoints extends RichMapFunction[String, (String, Point)] {
    var currentId: Long = 0;
    override def open(parameters: Configuration): Unit = super.open(parameters)
    override def map(line: String): (String, Point) = {
      currentId = currentId + 1
      // 0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,1,0,0,0,0,0,0,1,0,0,A
      val lineSplit = line.split(",")
      val doubles = lineSplit.slice(1, lineSplit.length - 1).map(_.toDouble)
      val label = lineSplit.last
      (label, Point(currentId, doubles))
    }
  }

  class MapCsvToMfogCluster extends RichMapFunction[String, MfogCluster] {
    override def map(line: String): MfogCluster = {
      val i = line.split('[').iterator
      val cl = i.next()
      val ce = i.next()
      //
      val center: Array[Double] = ce.take(ce.length-1).split(",").map(x => x.toDouble)
      val cluster: Array[String] = cl.take(cl.length-1).split(",")
      cluster match {
        case Array(id,label,category,matches,time,meanDistance,radius) => {
          // MfogCluster(id: Long, center: Point, variance: Double, label: String, category: String = MfogCluster.CATEGORY_NORMAL,
          // matches: Long = 0, time: Long = System.currentTimeMillis()) {
          MfogCluster(id.toInt, Point(id.toInt, center), radius.toDouble, label, category, matches.toLong)
          // (id.toInt, label, category, matches.toLong, time.toLong, meanDistance.toDouble, radius.toDouble, )
        }
      }
    }
  }

}
