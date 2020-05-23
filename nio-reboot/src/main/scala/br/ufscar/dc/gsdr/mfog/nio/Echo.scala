package br.ufscar.dc.gsdr.mfog.nio

import akka.stream._
import akka.stream.scaladsl._
import akka.stream.scaladsl.Tcp._
import akka.{ Done, NotUsed }
import akka.actor.ActorSystem
import akka.util.ByteString
import scala.concurrent._
import scala.concurrent.duration._
import java.nio.file.Paths


import scala.concurrent.Future

object Echo extends App {
  val host = "localhost"
  val port = 8888
  val x = Framing.lengthField(fieldLength = 4, maximumFrameLength = 1024*10)

  def server = {
    implicit val system: ActorSystem = ActorSystem("Echo-Server")
    implicit val materializer: ActorMaterializer = ActorMaterializer()
    Tcp().bind(host, port).runForeach { connection =>
      println(s"New connection from: ${connection.remoteAddress}")

      connection.handleWith(
        Flow[ByteString]
          .via(Framing.delimiter(ByteString("\n"), maximumFrameLength = 256, allowTruncation = true))
          .map(_.utf8String)
          .map(_ + "!!!\n")
          .prepend(Source.single("Hello"))
          .map(ByteString(_))
      )
    }
  }
  def client = {
    implicit val system: ActorSystem = ActorSystem("Echo-Client")
    implicit val materializer: ActorMaterializer = ActorMaterializer()

    Tcp().outgoingConnection(host, port).join(
      Flow[ByteString]
        .via(Framing.delimiter(ByteString("\n"), maximumFrameLength = 256, allowTruncation = true))
        .map(_.utf8String)
        .map(text => println("Server: " + text))
        .map(_ => scala.io.StdIn.readLine("> "))
        .via(
          Flow[String]
            .takeWhile(_ != "q")
            .prepend(Source.single("Hello"))
            .concat(Source.single("BYE"))
            .map(elem => ByteString(s"$elem\n"))
        )
    ).run()
  }

  new Thread(new Runnable() { override def run(): Unit = server }).start()
  new Thread(new Runnable() { override def run(): Unit = client }).start()
}
