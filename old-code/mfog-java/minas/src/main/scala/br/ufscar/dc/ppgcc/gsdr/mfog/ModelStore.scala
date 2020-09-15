package br.ufscar.dc.ppgcc.gsdr.mfog

import java.io.PrintStream
import java.net.ServerSocket

import grizzled.slf4j.Logger

import scala.collection.mutable
import scala.io.BufferedSource

object ModelStore {
  val LOG: Logger = Logger(getClass)
  val model: mutable.Buffer[String] = mutable.Buffer.empty
  var running: Boolean = true

  def main(args: Array[String]): Unit = {
    new Thread(new Runnable {
      override def run(): Unit = sender()
    }).start()
    receiver()
  }

  def sender(): Unit = {
    val senderServer = new ServerSocket(9997)
    val LOG: Logger = Logger(getClass)
    LOG.info("Sender ready")
    while (true) {
      val socket = senderServer.accept()
      val start = System.currentTimeMillis()
      LOG.info("sender connected")
      val out = new PrintStream(socket.getOutputStream)
      LOG.info(s"sending ${model.head}")
      model.foreach(x => out.println(x))
      out.flush()
      socket.close()
      LOG.info(s"sent ${model.size} items in ${(System.currentTimeMillis() - start) * 10e-4}s")
    }
    senderServer.close()
  }

  def receiver(): Unit = {
    val receiverServer = new ServerSocket(9998)
    LOG.info("Receiver ready")
    while (true) {
      val socket = receiverServer.accept()
      LOG.info("Receiver connected")
      LOG.info(s"appending to ${model.size}")
      val lines = new BufferedSource(socket.getInputStream).getLines().toSeq
      LOG.info(s"new lines ${lines.size} => ${lines.head}")
      model.appendAll(lines)
      socket.close()
      LOG.info(s"total ${model.size}")
    }
  }

}
