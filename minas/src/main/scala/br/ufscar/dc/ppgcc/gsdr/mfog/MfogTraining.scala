package br.ufscar.dc.ppgcc.gsdr.mfog

import java.io.{File, PrintStream}
import java.net.{InetAddress, Socket}
import java.time.LocalDateTime
import java.time.format.DateTimeFormatter

import br.ufscar.dc.ppgcc.gsdr.minas.MinasFlinkOffline.{indexInputFile, serialClustream, serialKMeans}
import br.ufscar.dc.ppgcc.gsdr.minas.kmeans.{MfogCluster, Point}
import br.ufscar.dc.ppgcc.gsdr.utils.CollectionsUtils.RichIterator
import br.ufscar.dc.ppgcc.gsdr.utils.FlinkUtils.RichSet
import org.slf4j.LoggerFactory
import grizzled.slf4j.Logger
import org.apache.flink.api.common.JobExecutionResult
import org.apache.flink.api.scala.{DataSet, ExecutionEnvironment}
import org.apache.flink.api.scala._
import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment
import org.apache.flink.streaming.api.functions.source.SourceFunction
import org.apache.flink.util.IOUtils

import scala.io.BufferedSource

object MfogTraining {
  val LOG: Logger = Logger(getClass)

  def main(args: Array[String]): Unit = {
    val jobName = this.getClass.getName
    val dateString = LocalDateTime.now.format(DateTimeFormatter.ISO_DATE_TIME).replaceAll(":", "-")
    LOG.info(s"jobName = $jobName")
    val outDir = s"./out/$jobName/$dateString/"
    val dir = new File(outDir)
    if (!dir.exists) {
      if (!dir.mkdirs) throw new RuntimeException(s"Output directory '$outDir'could not be created.")
    }
    val setEnv = ExecutionEnvironment.getExecutionEnvironment
    val env: StreamExecutionEnvironment = StreamExecutionEnvironment.getExecutionEnvironment

    val sourceSocket = new Socket(InetAddress.getByName("localhost"), 9999)
    LOG.info(s"connected = $sourceSocket")
    val influx = new BufferedSource(sourceSocket.getInputStream).getLines().toVector
    LOG.info(s"received  total ${influx.length} => ${influx.head} ${influx.last}")
    val in: Vector[(String, Point)] = influx.map(
      x => x.split(">") match {
          case Array(l, p) => (l, Point.fromCsv(p))
      }
    )
    LOG.info(s"received  total ${in.length} => ${in.head} ${in.last}")
    sourceSocket.close()
    val trainingSet: DataSet[(String, Point)] = setEnv.fromCollection(in).setParallelism(-1)

    val iterations = 10
    val k = 100
    val varianceThreshold = 1.0E-5

    //
    LOG.info(s"training.parallelism = ${trainingSet.parallelism}")
    trainingSet.writeAsText(s"$outDir/initial")
    //
    // serialKMeans(setEnv, outDir, k, trainingSet)
    val model: DataSet[MfogCluster] = serialClustream(setEnv, outDir, k, trainingSet)
    val modelSeq: Seq[MfogCluster] = model.collect()

    val modelStoreSocket = new Socket(InetAddress.getByName("localhost"), 9998)
    LOG.info(s"connected = $modelStoreSocket")
    val outStream = new PrintStream(modelStoreSocket.getOutputStream)
    modelSeq.foreach(x => outStream.println(x.csv))
    modelStoreSocket.close()

//    val flinkOut: JobExecutionResult = setEnv.execute(jobName)
//    flinkOut.getNetRuntime
//    flinkOut.getAccumulatorResult("model")
  }

  @SerialVersionUID(1L)
  object SocketTextStreamFunction {
    val DEFAULT_CONNECTION_RETRY_SLEEP = 500
    val DEFAULT_CONNECTION_TIMEOUT = 50
    val CONNECTION_TIMEOUT_TIME = 0

    def checkArgument(condition: Boolean, errorMessage: Any): Unit = {
      if (!condition) throw new IllegalArgumentException(String.valueOf(errorMessage))
    }

    def apply(hostname: InetAddress = InetAddress.getByName("localhost"), port: Int = 9999, maxNumRetries: Long = DEFAULT_CONNECTION_TIMEOUT, delayBetweenRetries: Long = DEFAULT_CONNECTION_RETRY_SLEEP): SocketTextStreamFunction = {
      checkArgument(port > 0 && port < 65536, "port is out of range")
      checkArgument(maxNumRetries >= -1, "maxNumRetries must be zero or larger (num retries), or -1 (infinite retries)")
      checkArgument(delayBetweenRetries >= 0, "delayBetweenRetries must be zero or positive")
      checkArgument(hostname != null, "hostname must not be null")
      new SocketTextStreamFunction(hostname, port, maxNumRetries, delayBetweenRetries)
    }
  }

  @SerialVersionUID(1L)
  class SocketTextStreamFunction(val hostname: InetAddress, val port: Int, val maxNumRetries: Long, val delayBetweenRetries: Long) extends SourceFunction[String] {
    private val LOG = LoggerFactory.getLogger(classOf[SocketTextStreamFunction])
    private var currentSocket: Socket = _
    private var isRunning = true

    @throws[Exception]
    override def run(ctx: SourceFunction.SourceContext[String]): Unit = {
      running(ctx)
    }

    def running(ctx: SourceFunction.SourceContext[String], attempt: Int = 0): Unit = {
      try {
        currentSocket = new Socket(hostname, port)
        val in: Iterator[String] = new BufferedSource(currentSocket.getInputStream).getLines()
        in.foreach(record => ctx.collect(record))
      } catch {
        case e: Exception => LOG.warn(s"Lost connection to server socket. $e")
      } finally {
        currentSocket.close()
      }
      currentSocket = null
      // if we dropped out of this loop due to an EOF, sleep and retry
      if (isRunning && maxNumRetries != -1 && attempt < maxNumRetries) {
        LOG.warn("Lost connection to server socket. Retrying in " + delayBetweenRetries + " msecs...")
        Thread.sleep(delayBetweenRetries)
        running(ctx, attempt + 1)
      }
    }

    override def cancel(): Unit = {
      isRunning = false
      // we need to close the socket as well, because the Thread.interrupt() function will
      // not wake the thread in the socketStream.read() method when blocked.
      if (this.currentSocket != null) IOUtils.closeSocket(this.currentSocket)
    }
  }
}
