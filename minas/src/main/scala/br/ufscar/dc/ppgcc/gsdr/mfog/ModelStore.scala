package br.ufscar.dc.ppgcc.gsdr.mfog

import java.io.PrintStream
import java.net.ServerSocket

import br.ufscar.dc.ppgcc.gsdr.minas.kmeans.{MfogCluster, Point}
import grizzled.slf4j.Logger

import scala.collection.mutable
import scala.io.BufferedSource

object ModelStore {
  def main(args: Array[String]): Unit = (new ModelStore).startServer()
}
class ModelStore {
  val LOG: Logger = Logger(getClass)
  val model: mutable.Buffer[String] = mutable.Buffer.empty

  def startServer(): Unit = {
    val senderServer = new ServerSocket(9997)
    new Thread(new Runnable {
      override def run(): Unit = {
        val LOG: Logger = Logger(getClass)
        LOG.info("Sender ready")
        // while (true) {
          val socket = senderServer.accept()
          LOG.info("sender connected")
          val out = new PrintStream(socket.getOutputStream)
          LOG.info(s"sending ${model.head}")
          model.foreach(x => out.println(x))
          out.flush()
          socket.close()
        // }
      }
    })
    //
    val receiverServer = new ServerSocket(9998)
    LOG.info("Receiver ready")
    while (true) {
      val socket = receiverServer.accept()
      LOG.info("Receiver connected")
      LOG.info(s"appending to ${model.size}")
      val lines = new BufferedSource(socket.getInputStream).getLines().toSeq
      LOG.info(s"new lines ${lines.size}")
      model.appendAll(lines)
      socket.close()
      LOG.info(s"total ${model.size}")
    }
  }

}
