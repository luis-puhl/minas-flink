package br.ufscar.dc.ppgcc.gsdr.mfog

import java.io.{BufferedWriter, File, FileWriter, PrintStream}
import java.net.{InetAddress, Socket}
import java.time.LocalDateTime
import java.time.format.DateTimeFormatter

import grizzled.slf4j.Logger
import org.apache.flink.api.scala._
import org.apache.flink.api.common.functions.{MapFunction, ReduceFunction, RichMapFunction}
import org.apache.flink.configuration.Configuration
import org.apache.flink.streaming.api.datastream.BroadcastStream
import org.apache.flink.streaming.api.scala.DataStream
import org.apache.flink.streaming.api.scala.StreamExecutionEnvironment
import org.apache.flink.streaming.api.functions.co.{BroadcastProcessFunction, CoProcessFunction}
import org.apache.flink.util.Collector
import org.json.JSONObject

import scala.collection.{immutable, mutable}
import scala.io.BufferedSource

object Classifier {
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

    // baseline(outDir, jobName) // sem minas (distancias) avalia o flink (somente IO)
    // 8.to(1, -1).foreach(par => run(outDir, jobName, par))
    val (model, data) = setUpWithFullStatic()
    // 8.to(1, -1).foreach(par => runWithStaticModel(outDir, jobName, par, model))
    // (8 to 1).foreach(par => runWithFullStatic(outDir, jobName, par, model, data))
    plainScala(outDir, model, data)
  }

  class R(val p: Point, val c: Cluster, val d: Double, val l: String, val t: Long) {
    override def toString: String = s"$d,$l,$t,$p,$c"
  }

  /**
   * 14:48:49 INFO  mfog.Classifier$ jobName = br.ufscar.dc.ppgcc.gsdr.mfog.Classifier$
   * 14:50:21 INFO  mfog.Classifier$ setup in 92.634s
   * 14:51:12 INFO  mfog.Classifier$ plainScala in 50.645s
   * 14:51:12 INFO  mfog.Classifier$ plainScala average latency = 70.978 seconds

     18:03:01 INFO  mfog.Classifier$ jobName = br.ufscar.dc.ppgcc.gsdr.mfog.Classifier$
    18:03:22 INFO  mfog.Classifier$ setup in 21.158s
    18:04:52 INFO  mfog.Classifier$ plainScala in 89.49s
    18:04:52 INFO  mfog.Classifier$ plainScala average latency = 52.719 seconds

    Process finished with exit code 0

   *
   * @param outDir
   * @param model
   * @param data
   */
  def plainScala(outDir: String, model: immutable.Seq[Cluster], data: immutable.Seq[Point]): Unit = {
    val start = System.currentTimeMillis()
    val fp = new java.io.PrintWriter(new File(outDir + "plainScala"))
    val outStream = data.par.map(x => {
      val (d, c) = model.map(c => (x.euclideanDistance(c.center), c)).minBy(_._1)
      val l = if (d <= c.variance) c.label else "unk"
      new R(x, c, d, l, System.currentTimeMillis() - x.time)
    })
    outStream.foreach(out => fp.println(out.toString))
    fp.close()
    val avg = (outStream.map(_.t).sum / outStream.size) * 10e-4
    LOG.info(s"plainScala in ${(System.currentTimeMillis() - start) * 10e-4}s")
    LOG.info(s"plainScala average latency = $avg seconds")
  }

  /**
   * 14:50:21 INFO  mfog.Classifier$ setup in 92.634s
   * @return
   */
  def setUpWithFullStatic(): (immutable.Seq[Cluster], immutable.Seq[Point]) = {
    val start = System.currentTimeMillis()
    val model: immutable.Seq[Cluster] = {
      val socket = new Socket(InetAddress.getByName("localhost"), 9997)
      val values = new BufferedSource(socket.getInputStream).getLines().toVector.map(c => Cluster.fromJson(c))
      socket.close()
      values
    }
    val data: immutable.Seq[Point] = {
      val socket = new Socket(InetAddress.getByName("localhost"), 9996)
      val values = new BufferedSource(socket.getInputStream).getLines().toVector.map(c => Point.fromJson(c))
      socket.close()
      values
    }
    LOG.info(s"setup in ${(System.currentTimeMillis() - start) * 10e-4}s")
    (model, data)
  }

  /**
   * 
   * 14:16:08 INFO  mfog.Classifier$ jobName = br.ufscar.dc.ppgcc.gsdr.mfog.Classifier$
   * 14:18:05 INFO  mfog.Classifier$ setup in 116.757s
   * 14:23:40 INFO  mfog.Classifier$ Ran par=1 in 325.084s
   * 14:26:46 INFO  mfog.Classifier$ Ran par=2 in 184.172s
   * 14:28:59 INFO  mfog.Classifier$ Ran par=3 in 130.406s
   * 14:30:52 INFO  mfog.Classifier$ Ran par=4 in 110.994s
   * 14:32:35 INFO  mfog.Classifier$ Ran par=5 in 100.105s
   * 14:34:04 INFO  mfog.Classifier$ Ran par=6 in 86.264s
   * 14:35:29 INFO  mfog.Classifier$ Ran par=7 in 82.671s
   * 14:37:04 INFO  mfog.Classifier$ Ran par=8 in 93.032s
   */
  def runWithFullStatic(outDir: String, jobName: String, par: Int, model: immutable.Seq[Cluster], data: immutable.Seq[Point]): Unit = {
    class R(val p: Point, val c: Cluster, val d: Double, val l: String, val t: Long) {
      override def toString: String = s"$d,$l,$t,$p,$c"
    }
    val env: StreamExecutionEnvironment = StreamExecutionEnvironment.getExecutionEnvironment
    env.setParallelism(par)

    val outStream = env
      .fromCollection(data)
      .connect(env.fromElements[Seq[Cluster]](model).broadcast())
      .process(new BroadcastProcessFunction[Point, Seq[Cluster], R]() {
        type ReadOnlyContext = BroadcastProcessFunction[Point, Seq[Cluster], R]#ReadOnlyContext
        type Context = BroadcastProcessFunction[Point, Seq[Cluster], R]#Context
        var model: mutable.Buffer[Cluster] = mutable.Buffer.empty
        var backlog: mutable.Buffer[Point] = mutable.Buffer.empty

        def classify(x: Point): R = {
          val (d, c) = model.map(c => (x.euclideanDistance(c.center), c)).minBy(_._1)
          val l = if (d <= c.variance) c.label else "unk"
          new R(x, c, d, l, System.currentTimeMillis() - x.time)
        }

        def processElement(x: Point, ctx: ReadOnlyContext, out: Collector[R]): Unit = {
          if (model.isEmpty) {
            backlog.append(x)
          } else {
            out.collect(classify(x))
          }
        }

        def processBroadcastElement(value: Seq[Cluster], ctx: Context, out: Collector[R]): Unit = {
          if (value.nonEmpty) {
            model = mutable.Buffer.empty
            model.appendAll(value)
            if (backlog.nonEmpty) {
              backlog.foreach(x => out.collect(classify(x)))
            }
          }
        }
      })
    outStream.writeAsText(s"$outDir/classifier-$par")
    val latency = outStream.map(_.t)
    // latency.writeAsText(s"$outDir/latency-$par")
    val windowSize: Long = 10000
    latency
      .countWindowAll(windowSize)
      .reduce((a, b) => a + b)
      .map(_ / windowSize)
      .writeAsText(s"$outDir/latency-avg-$par")

    // LOG.info(s"Ready to run with par=$par")
    val start = System.currentTimeMillis()
    env.execute(jobName)
    LOG.info(s"Ran par=$par in ${(System.currentTimeMillis() - start) * 10e-4}s")
  }

  /**
   * 15:59:55 INFO  mfog.Classifier$ setup in 93.898s
   * 16:01:29 INFO  mfog.Classifier$ Ran par=8 in 87.83s
   * 16:03:01 INFO  mfog.Classifier$ Ran par=7 in 91.461s
   * 16:04:34 INFO  mfog.Classifier$ Ran par=6 in 93.796s
   * 16:06:22 INFO  mfog.Classifier$ Ran par=5 in 107.389s
   * 16:08:14 INFO  mfog.Classifier$ Ran par=4 in 112.238s
   * 16:10:30 INFO  mfog.Classifier$ Ran par=3 in 136.213s
   * 16:13:36 INFO  mfog.Classifier$ Ran par=2 in 185.903s
   * 16:19:25 INFO  mfog.Classifier$ Ran par=1 in 348.925s
   *
   * @param outDir
   * @param jobName
   * @param par
   * @param model
   */
  def runWithStaticModel(outDir: String, jobName: String, par: Int,  model: immutable.Seq[Cluster]): Unit = {
    class R(val p: Point, val c: Cluster, val d: Double, val l: String, val t: Long) {
      override def toString: String = s"$d,$l,$t,$p,$c"
    }
    val env: StreamExecutionEnvironment = StreamExecutionEnvironment.getExecutionEnvironment
    env.setParallelism(par)

    val outStream = env.socketTextStream("localhost", 9996)
      .map[Point]((x: String) => Point.fromJson(x))
      .connect(env.fromElements[Seq[Cluster]](model).broadcast())
      .process(new BroadcastProcessFunction[Point, Seq[Cluster], R]() {
        type ReadOnlyContext = BroadcastProcessFunction[Point, Seq[Cluster], R]#ReadOnlyContext
        type Context = BroadcastProcessFunction[Point, Seq[Cluster], R]#Context
        var model: mutable.Buffer[Cluster] = mutable.Buffer.empty
        var backlog: mutable.Buffer[Point] = mutable.Buffer.empty

        def classify(x: Point): R = {
          val (d, c) = model.map(c => (x.euclideanDistance(c.center), c)).minBy(_._1)
          val l = if (d <= c.variance) c.label else "unk"
          new R(x, c, d, l, System.currentTimeMillis() - x.time)
        }

        def processElement(x: Point, ctx: ReadOnlyContext, out: Collector[R]): Unit = {
          if (model.isEmpty) {
            backlog.append(x)
          } else {
            out.collect(classify(x))
          }
        }

        def processBroadcastElement(value: Seq[Cluster], ctx: Context, out: Collector[R]): Unit = {
          if (value.nonEmpty) {
            model = mutable.Buffer.empty
            model.appendAll(value)
            if (backlog.nonEmpty) {
              backlog.foreach(x => out.collect(classify(x)))
            }
          }
        }
      })
    outStream.writeAsText(s"$outDir/classifier-$par")
    val latency = outStream.map(_.t)
    latency.writeAsText(s"$outDir/latency-$par")
    val windowSize: Long = 10000
    latency
      .countWindowAll(windowSize)
      .reduce((a, b) => a + b)
      .map(_ / windowSize)
      .writeAsText(s"$outDir/latency-avg-$par")

    val start = System.currentTimeMillis()
    env.execute(jobName)
    val end = System.currentTimeMillis()
    LOG.info(s"Ran runWithStaticModel par=$par in ${(end - start) * 10e-4}s")
  }

  /**
   * 14:54:05 INFO  mfog.Classifier$ Ready to run baseline
   * 14:55:57 INFO  mfog.Classifier$ Ran baseline in 111.805s
   *
   * @param outDir
   * @param jobName
   */
  def baseline(outDir: String, jobName: String): Unit = {
    val env: StreamExecutionEnvironment = StreamExecutionEnvironment.getExecutionEnvironment

    env.socketTextStream("localhost", 9996)
      .map[Point](new MapFunction[String, Point]() {
        override def map(value: String): Point = Point.fromJson(value)
      })
      .connect(
        env.socketTextStream("localhost", 9997)
          .map[Seq[Cluster]](new RichMapFunction[String, Seq[Cluster]]() {
            var model: mutable.Buffer[Cluster] = mutable.Buffer.empty

            override def open(parameters: Configuration): Unit = {
              super.open(parameters)
              model = mutable.Buffer.empty
            }

            override def map(value: String): Seq[Cluster] = {
              val cl = Cluster.fromJson(new JSONObject(value))
              model.append(cl)
              model
            }
          })
          .broadcast()
      )
      .process(new BroadcastProcessFunction[Point, Seq[Cluster], (Long, Long)]() {
        // type Context = CoProcessFunction[Point, Seq[Cluster], (Long, Long)]#Context
        type ReadOnlyContext = BroadcastProcessFunction[Point, Seq[Cluster], (Long, Long)]#ReadOnlyContext
        type Context = BroadcastProcessFunction[Point, Seq[Cluster], (Long, Long)]#Context

        override def processElement(x: Point, ctx: ReadOnlyContext, out: Collector[(Long, Long)]): Unit = {
          out.collect((System.currentTimeMillis() - x.time, 0))
        }

        override def processBroadcastElement(value: Seq[Cluster], ctx: Context, out: Collector[(Long, Long)]): Unit = {
          out.collect((0, System.currentTimeMillis() - value.last.time))
        }
      })
      .writeAsText(s"$outDir/baseline")
      // a -> |
      // b -> | p -> baseline.txt
    LOG.info(s"Ready to run baseline")
    val start = System.currentTimeMillis()
    env.execute(jobName)
    val end = System.currentTimeMillis()
    LOG.info(s"Ran baseline in ${(end - start) * 10e-4}s")
  }

  /**
   * 15:33:20 INFO  mfog.Classifier$ Ran par=8 in 101.63s
   * 15:34:48 INFO  mfog.Classifier$ Ran par=7 in 87.35300000000001s
   * 15:36:17 INFO  mfog.Classifier$ Ran par=6 in 89.44200000000001s
   * 15:37:47 INFO  mfog.Classifier$ Ran par=5 in 89.737s
   * 15:39:16 INFO  mfog.Classifier$ Ran par=4 in 89.217s
   * 15:40:41 INFO  mfog.Classifier$ Ran par=3 in 84.538s
   * 15:42:36 INFO  mfog.Classifier$ Ran par=2 in 115.894s
   * 15:47:59 INFO  mfog.Classifier$ Ran par=1 in 322.672s
   *
   * @param outDir
   * @param jobName
   * @param par
   */
  def run(outDir: String, jobName: String, par: Int): Unit = {
    class R(val p: Point, val c: Cluster, val d: Double, val l: String, val t: Long) {
      override def toString: String = s"$d,$l,$t,$p,$c"
    }
    val env: StreamExecutionEnvironment = StreamExecutionEnvironment.getExecutionEnvironment
    env.setParallelism(par)

    val outStream = env.socketTextStream("localhost", 9996)
      .map[Point]((x: String) => Point.fromJson(x))
      .connect(env.socketTextStream("localhost", 9997)
        .map[Seq[Cluster]](new RichMapFunction[String, Seq[Cluster]]() {
          var model: mutable.Buffer[Cluster] = mutable.Buffer.empty

          override def open(parameters: Configuration): Unit = {
            super.open(parameters)
            model = mutable.Buffer.empty
          }

          override def map(value: String): Seq[Cluster] = {
            val cl = Cluster.fromJson(new JSONObject(value))
            model.append(cl)
            model
          }
        })
        .broadcast()
      )
      .process(new BroadcastProcessFunction[Point, Seq[Cluster], R]() {
        type ReadOnlyContext = BroadcastProcessFunction[Point, Seq[Cluster], R]#ReadOnlyContext
        type Context = BroadcastProcessFunction[Point, Seq[Cluster], R]#Context
        var model: mutable.Buffer[Cluster] = mutable.Buffer.empty
        var backlog: mutable.Buffer[Point] = mutable.Buffer.empty

        def classify(x: Point): R = {
          val (d, c) = model.map(c => (x.euclideanDistance(c.center), c)).minBy(_._1)
          val l = if (d <= c.variance) c.label else "unk"
          new R(x, c, d, l, System.currentTimeMillis() - x.time)
        }

        def processElement(x: Point, ctx: ReadOnlyContext, out: Collector[R]): Unit = {
          if (model.isEmpty) {
            backlog.append(x)
          } else {
            out.collect(classify(x))
          }
        }

         def processBroadcastElement(value: Seq[Cluster], ctx: Context, out: Collector[R]): Unit = {
          if (value.nonEmpty) {
            model = mutable.Buffer.empty
            model.appendAll(value)
            if (backlog.nonEmpty) {
              backlog.foreach(x => out.collect(classify(x)))
            }
          }
        }
      })
    outStream.writeAsText(s"$outDir/classifier-$par")
    val latency = outStream.map(_.t)
    latency.writeAsText(s"$outDir/latency-$par")
    val windowSize: Long = 10000
    latency
      .countWindowAll(windowSize)
      .reduce((a, b) => a + b)
      .map(_ / windowSize)
      .writeAsText(s"$outDir/latency-avg-$par")

    val start = System.currentTimeMillis()
    env.execute(jobName)
    val end = System.currentTimeMillis()
    LOG.info(s"Ran par=$par in ${(end - start) * 10e-4}s")
  }

    /**
     - usar somente um modelo sem atualização (MVP)
     - tabela de tempos por parametro de paralelismo
        * minas leva 54s (online)
     - mover para os RPI para extração de tempo também.
    x1, x2, x3 ...
    t1, t2, t3 ...
                        |-> 8 processos
    m1, m2, m3 ...
    tm1, tm2, tm3 ...

    |x1, model|
    */
}
