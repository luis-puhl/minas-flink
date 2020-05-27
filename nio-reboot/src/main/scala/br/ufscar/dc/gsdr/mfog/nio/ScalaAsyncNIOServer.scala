package br.ufscar.dc.gsdr.mfog.nio

import java.net.{InetSocketAddress, StandardSocketOptions}
import java.nio.ByteBuffer
import java.nio.channels.{AsynchronousServerSocketChannel, AsynchronousSocketChannel}
import java.util.concurrent.Future

import scala.concurrent.ExecutionContext.Implicits.global
import scala.concurrent.{Promise, blocking}
import scala.util.{Failure, Success, Try}

object ScalaAsyncNIOServer extends App {
  val host = "localhost"
  val port = 8888

  def server(): Unit = {
    /**
     * @see <a href="http://www.programmingopiethehokie.com/2014/10/scala-asynchronous-io-echo-server.html">Source</a>
     */
    val listener: AsynchronousServerSocketChannel = AsynchronousServerSocketChannel.open

    listener.setOption[java.lang.Boolean](StandardSocketOptions.SO_REUSEADDR, true)
    listener.bind(new InetSocketAddress(host, port))

    val buffer: ByteBuffer = ByteBuffer.allocateDirect(1024)
    while (true) {
      val p: Promise[AsynchronousSocketChannel] =
      p.complete(Try { listener.accept.get})
      val message: concurrent.Future[AsynchronousSocketChannel] = p.future map {
        connection =>
          connection.read(buffer).get
          connection
      }
      message.onComplete {
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
    val clientChan = AsynchronousSocketChannel.open
    clientChan.connect(new InetSocketAddress(host, port)).get
    val buffer = ByteBuffer.allocateDirect(1024)

  }

  new Thread(new Runnable() { override def run(): Unit = server }).start()
  new Thread(new Runnable() { override def run(): Unit = client }).start()
}
