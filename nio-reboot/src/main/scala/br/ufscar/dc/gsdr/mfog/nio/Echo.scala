package br.ufscar.dc.gsdr.mfog.nio

import java.io.DataOutputStream

import akka.stream._
import akka.stream.scaladsl._
import akka.stream.scaladsl.Tcp._
import akka.{Done, NotUsed}
import akka.actor.ActorSystem
import akka.util.ByteString

import scala.concurrent._
import scala.concurrent.duration._
import java.nio.file.Paths

import scala.concurrent.Future

object Echo extends App {
  val host = "localhost"
  val port = 8888
  val framing = Framing.lengthField(fieldLength = 4, maximumFrameLength = 1024*10)

  case class Cluster(id: Int, label: String, value: Vector[Float]) {
    def toByteString: ByteString = {
      val builder = ByteString.newBuilder
      val os = new DataOutputStream(builder.asOutputStream)
      os.writeInt(id)
      os.writeUTF(label)
      os.writeInt(value.length)
      value.foreach(os.writeFloat)
      builder.result()
    }
  }
  def fromByteString(bytes: ByteString): Cluster = Cluster(0, "lbl", Vector.fill(10)(0.1f))

  def server = {
    implicit val system: ActorSystem = ActorSystem("Echo-Server")
    implicit val materializer: ActorMaterializer = ActorMaterializer()
    Tcp().bind(host, port).runForeach { connection =>
      println(s"New connection from: ${connection.remoteAddress}")

      connection.handleWith(
        Flow[ByteString]
          .via(framing)
          .map(fromByteString)
          .map(x => Cluster(x.id, x.label + "!", x.value))
          .prepend(Source.single(Cluster(0, "Hello", Vector.empty)))
          .map(_.toByteString)
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
        // .via(framing)
        // .map(fromByteString)
        .map(text => println("Server: " + text))
        .map(_ => scala.io.StdIn.readLine("> "))
        .via(
          Flow[String]
            .takeWhile(_ != "q")
            .prepend(Source.single("Hello"))
            .concat(Source.single("BYE"))
            .map(elem => ByteString(s"$elem\n"))
            // .map(_ => Cluster(1, _, Vector.empty).toByteString)
        )
    ).run()
  }

  new Thread(new Runnable() { override def run(): Unit = server }).start()
  new Thread(new Runnable() { override def run(): Unit = client }).start()
}
