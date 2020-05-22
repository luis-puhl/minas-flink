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
  implicit val system: ActorSystem = ActorSystem("Echo-TCP")
  implicit val materializer: ActorMaterializer = ActorMaterializer()

  val host = "localhost"
  val port = 8888

  println(s"To run: \njava -cp target/*:../ref-git/flink-1.10.0/lib/*     br.ufscar.dc.gsdr.mfog.nio.Echo &\n nc $host $port\nand say something.")

  val connections: Source[IncomingConnection, Future[ServerBinding]] = Tcp().bind(host, port)
  connections.runForeach { connection =>
    println(s"New connection from: ${connection.remoteAddress}")

    val echo = Flow[ByteString]
      .via(Framing.delimiter(ByteString("\n"), maximumFrameLength = 256, allowTruncation = true))
      .map(_.utf8String)
      .map(_ + "!!!\n")
      .map(ByteString(_))

    connection.handleWith(echo)
  }
}
