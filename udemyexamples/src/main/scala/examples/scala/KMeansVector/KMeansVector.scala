package examples.scala.KMeansVector

import org.apache.flink.api.common.serialization.SimpleStringSchema
import org.apache.flink.api.scala._
import org.apache.flink.core.fs.FileSystem
import org.apache.flink.streaming.api.functions.sink.SocketClientSink
import org.apache.flink.streaming.api.scala.{DataStream, StreamExecutionEnvironment}
import org.apache.flink.streaming.api.windowing.assigners.TumblingProcessingTimeWindows
import org.apache.flink.streaming.api.windowing.time.Time
import org.apache.flink.util.Collector

import scala.util.{Failure, Try}

object KMeansVector extends App {
  val env = StreamExecutionEnvironment.getExecutionEnvironment
  val term$: DataStream[String] = env.socketTextStream("127.0.0.1", 3232)
  val kddString$: DataStream[String] =
    // env.socketTextStream("127.0.0.1", 3232)
    // env.readTextFile("./tmpfs/kddcup.data")
    env.readTextFile("./kdd.ics.uci.edu/databases/kddcup99/kddcup.data")
  // val kdd$: DataStream[Kdd.MaybeEntry] = kddString$.map { line => Kdd.fromLine(line) }
  val words: DataStream[(Int, String)] = kddString$.flatMap { (line, out) => {
    val values = line.split(",")
    Kdd.symbolicIndexes.foreach(i => out.collect( (1, s"${i._2} ${values(i._1)}") ))
  }}
  val count: DataStream[(Int, String)] = words
    .keyBy(1)
    .sum(0)
  // count.print()
  count.writeAsText("out", FileSystem.WriteMode.OVERWRITE)
  count.keyBy(0).reduce(
    (acc: (Int, String), input: (Int, String)) => (input._1 + acc._1, acc._2)
  ).print()
  env.execute()
}
