package br.ufscar.dc.ppgcc.gsdr.mfog

import java.io.{File, PrintStream}
import java.net.{InetAddress, Socket}

import grizzled.slf4j.Logger

import scala.io.BufferedSource
import scala.reflect.io.Path

object TrainingStatic {
  val LOG: Logger = Logger(getClass)
  def main(args: Array[String]): Unit = {
    val path = Path.string2path("datasets/model/offline-clean.csv")
    val modelPath: String = "datasets/model/offline-clean.csv".replaceAll("/", "\\" + File.separator)
    assert(path.toString() == modelPath)
    val bufferedSource: BufferedSource = io.Source.fromFile(modelPath)
    val modelSeq = bufferedSource.getLines.drop(1).map(Cluster.fromMinasCsv).toSeq
    LOG.info(s"modelSeq = ${modelSeq.size} => ${modelSeq.head.json.toString}")

    val modelStoreSocket = new Socket(InetAddress.getByName("localhost"), 9998)
    LOG.info(s"connected = $modelStoreSocket")
    val outStream = new PrintStream(modelStoreSocket.getOutputStream)
    modelSeq.foreach(x => outStream.println(x.json.toString))
    outStream.flush()
    modelStoreSocket.close()
    LOG.info(s"sent = ${modelSeq.size}")
  }
}
