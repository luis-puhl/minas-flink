package br.ufscar.dc.gsdr.mfog.nio

import java.net.{InetSocketAddress, StandardSocketOptions}
import java.nio.ByteBuffer
import java.nio.channels.{AsynchronousServerSocketChannel, AsynchronousSocketChannel}

import scala.concurrent.ExecutionContext.Implicits.global
import scala.concurrent.{blocking, Promise}
import scala.util.{Failure, Success, Try}

object ScalaAsyncNIOServer extends App {

  def server(): Unit = {
    val listener = AsynchronousServerSocketChannel.open

    listener.setOption[java.lang.Boolean](StandardSocketOptions.SO_REUSEADDR, true);
    listener.bind(new InetSocketAddress("localhost", 8080));

    val buffer = ByteBuffer.allocateDirect(1024)
    while (true) {
      val p = Promise[AsynchronousSocketChannel]
      p.complete(Try { listener.accept.get})
      val message = p.future map {
        connection =>
          connection.read(buffer).get
          connection
      }

      message onComplete {
        case Success(connection) =>
          buffer.flip
          blocking {
            connection.write(buffer)
          }
          connection.close()
        case Failure(t) => t.printStackTrace()
      }
      buffer.clear()
    }
  }
  def client(): Unit = {
    AsynchronousSocketChannel
  }

  new Thread(new Runnable() { override def run(): Unit = server }).start()
  new Thread(new Runnable() { override def run(): Unit = client }).start()
}
