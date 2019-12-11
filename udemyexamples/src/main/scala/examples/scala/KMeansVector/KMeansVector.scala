package examples.scala.KMeansVector

import org.apache.flink.api.common.functions.AggregateFunction
import org.apache.flink.api.common.serialization.SimpleStringSchema
import org.apache.flink.api.scala._
import org.apache.flink.core.fs.FileSystem
import org.apache.flink.streaming.api.functions.sink.SocketClientSink
import org.apache.flink.streaming.api.scala.function.WindowFunction
import org.apache.flink.streaming.api.scala.{DataStream, StreamExecutionEnvironment}
import org.apache.flink.util.Collector

import scala.util.{Failure, Try}

object KMeansVector extends App {
  val env = StreamExecutionEnvironment.getExecutionEnvironment
  val symbolicIndexes = Map[Int, String](
    2  -> "protocol_type",
    3  -> "service",
    4  -> "flag",
    7  -> "land",
    12 -> "logged_in",
    21 -> "is_host_login",
    22 -> "is_guest_login",
    42 -> "label"
  ).map(x => (x._1 - 1 -> x._2))
  val term$: DataStream[String] = env.socketTextStream("127.0.0.1", 3232)
  val kddString$: DataStream[String] =
    // env.socketTextStream("127.0.0.1", 3232)
    // env.readTextFile("./tmpfs/kddcup.data")
    env.readTextFile("./kdd.ics.uci.edu/databases/kddcup99/kddcup.data")
  // val kdd$: DataStream[Kdd.MaybeEntry] = kddString$.map { line => Kdd.fromLine(line) }
  val words: DataStream[(String, String)] = kddString$.flatMap { (line, out) => {
    val values = line.split(",")
    symbolicIndexes.foreach((i: (Int, String)) => out.collect( (i._2, values(i._1)) ))
  }}
  type CountT = (String, Set[(String, String)])
  val count: DataStream[CountT] = words
    .map(t => (t._1, Set(t)))
    .keyBy(0)
    // .reduce( (acc: CountT, input: CountT) => (acc._1, acc._2 ++ input._2) )
    .countWindow(1000)
    .reduce((value: CountT, acc: CountT) => (acc._1, acc._2 ++ value._2))
//    .aggregate[CountT, CountT](
//      (value: CountT, acc: CountT) => (acc._1, acc._2 ++ value._2)
////      new AggregateFunction[CountT, CountT] {
////        override def createAccumulator(): CountT = ("", Set())
////        override def add(value: CountT, acc: CountT): CountT = (acc._1, acc._2 ++ value._2)
////        override def getResult(acc: CountT): CountT = acc
////        override def merge(a: CountT, b: CountT): CountT = (a._1, a._2 ++ b._2)
////      }, (key: String, window, input: Iterable[CountT], out: Collector[CountT]) => input foreach out.collect
//    )
  // count.print()
  count.writeAsText("out", FileSystem.WriteMode.OVERWRITE)
  env.execute()
}
