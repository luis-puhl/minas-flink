package br.ufscar.dc.ppgcc.gsdr.mfog

import java.io.File
import java.time.LocalDateTime
import java.time.format.DateTimeFormatter

import grizzled.slf4j.Logger
import org.apache.flink.api.common.functions.{MapFunction, RichMapFunction}
import org.apache.flink.configuration.Configuration
import org.apache.flink.streaming.api.datastream.DataStreamSource
import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment
import org.apache.flink.streaming.api.functions.co.CoProcessFunction
import org.apache.flink.util.Collector
import org.json.JSONObject

import scala.collection.mutable

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
    val env: StreamExecutionEnvironment = StreamExecutionEnvironment.getExecutionEnvironment

    val modelSource: DataStreamSource[String] = env.socketTextStream("localhost", 9997)
    modelSource.writeAsText(s"$outDir/classifier-model")
    val modelStore = modelSource.map[Seq[Cluster]](new RichMapFunction[String, Seq[Cluster]]() {
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
    val testSrc = env.socketTextStream("localhost", 9996)
    testSrc.writeAsText(s"$outDir/classifier-test")
    val testStream = testSrc.map[Point](new MapFunction[String, Point]() {
      override def map(value: String): Point = Point.fromJson(value)
    })

//    val centroids: Seq[Cluster] = {
//      val soc = new Socket(InetAddress.getByName("localhost"), 9997)
//      val in = new BufferedSource(soc.getInputStream).getLines().map(p => Cluster.fromJson(new JSONObject(p))).toVector
//      soc.close()
//      in
//    }
    type R = (Long, Long, String, String, Double, Seq[Double])
    (1 to 8).foreach(par => {
      testStream
        .setParallelism(par)
        .connect(modelStore)
        .process(new CoProcessFunction[Point, Seq[Cluster], R]() {
          type Context = CoProcessFunction[Point, Seq[Cluster], R]#Context
          var model: mutable.Buffer[Cluster] = mutable.Buffer.empty
          var backlog: mutable.Buffer[Point] = mutable.Buffer.empty
//          override def open(parameters: Configuration): Unit = {
//            super.open(parameters)
//            model = mutable.Buffer.empty
//          }
          override def processElement1(x: Point, ctx: Context, out: Collector[R]): Unit = {
            if (model.isEmpty) {
              backlog.append(x)
            } else {
              val min = model.map(c => (x.euclideanDistance(c.center), c)).minBy(_._1)
              val result = (x.id, System.currentTimeMillis(), min._2.label, min._2.category, min._1, x.value)
              out.collect(result)
            }
          }

          override def processElement2(value: Seq[Cluster], ctx: Context, out: Collector[R]): Unit = {
            if (value.nonEmpty) {
              model = mutable.Buffer.empty
              model.appendAll(value)
              if (backlog.nonEmpty) {
                backlog.foreach(x => processElement1(x, ctx, out))
              }
            }
          }
        })
        .setParallelism(par)
        .writeAsText(s"$outDir/classifier-$par")
    })

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

//    testStream.keyBy(new KeySelector[Point, Int]() {
//      override def getKey(value: Point): Int = 0
//    })
    //
//    testStream
//      .map(new RichMapFunction[Point, (Long, Cluster, mutable.IndexedSeq[Point], Long)] {
//        val model: mutable.Buffer[Cluster] = mutable.Buffer.empty
//        override def open(parameters: Configuration): Unit = {
//          super.open(parameters)
//          model.appendAll(getRuntimeContext.getBroadcastVariable[Cluster]("centroids").asScala)
//        }
//
//      def map(p: Point): (Long, Cluster, mutable.IndexedSeq[Point], Long) = {
//        val minDistance: (Double, Cluster) = model.map(c => (p.euclideanDistance(c.center), c)).minBy(_._1)
//        (minDistance._2.id, minDistance._2, mutable.IndexedSeq(p), 1L)
//      }
//    })
      // .withBroadcastSet(currentCentroids, "centroids")
    env.execute(jobName)
  }
}
