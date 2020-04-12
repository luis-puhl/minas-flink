package br.ufscar.dc.ppgcc.gsdr.mfog

import java.io.{File, PrintStream}
import java.net.{InetAddress, Socket}
import java.nio.file.Files

import br.ufscar.dc.ppgcc.gsdr.mfog.SourceKyoto.MapCsvToMfogCluster
import grizzled.slf4j.Logger
import org.apache.flink.api.scala.ExecutionEnvironment

import scala.io.BufferedSource

object TrainingStatic {
  val LOG: Logger = Logger(getClass)
  def main(args: Array[String]): Unit = {
    val modelPath: String = "datasets/model/offline-clean.csv".replaceAll("/", "\\" + File.separator)
    val bufferedSource: BufferedSource = io.Source.fromFile(modelPath)
    val mapper = new MapCsvToMfogCluster()
    val modelSeq = bufferedSource.getLines.drop(1).map(mapper.map).toSeq
    LOG.info(s"modelSeq = ${modelSeq.size}")

    val modelStoreSocket = new Socket(InetAddress.getByName("localhost"), 9998)
    LOG.info(s"connected = $modelStoreSocket")
    val outStream = new PrintStream(modelStoreSocket.getOutputStream)
    LOG.info(s"sending = ${modelSeq.head.json.toString}")
    modelSeq.foreach(x => outStream.println(x.json.toString))
    outStream.flush()
    modelStoreSocket.close()
    LOG.info(s"sent = ${modelSeq.size}")
  }
}
