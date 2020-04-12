package br.ufscar.dc.ppgcc.gsdr.mfog

import java.io.PrintStream
import java.net.ServerSocket

import br.ufscar.dc.ppgcc.gsdr.minas.kmeans.{MfogCluster, Point}
import grizzled.slf4j.Logger

import scala.io.BufferedSource

object ModelStore {
  def main(args: Array[String]): Unit = {
    (new ModelStore).startServer
  }
}
class ModelStore {
  val LOG: Logger = Logger(getClass)
  val modelSeq: Seq[MfogCluster]

  def startServer: Unit = {
    val receivingServer = new ServerSocket(9998)
    LOG.info("receivingServer ready")
    while (true) {
      val s = receivingServer.accept()
      new Runnable {
        override def run(): Unit = {
          LOG.info("connected")
          val in = new BufferedSource(s.getInputStream).getLines()
          val out = new PrintStream(s.getOutputStream)
          def toMsg(x: (String, Point)): String = {
            val (l, p) = x
            s"$l>${p.csv}"
          }
          LOG.info(s"sending ${toMsg(trainingSet.head)}")
          trainingSet.foreach(x => out.println(toMsg(x)))
          out.flush()
          s.close()
        }
      }.run()
    }
  }

}
