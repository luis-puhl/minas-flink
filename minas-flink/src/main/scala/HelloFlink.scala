import org.apache.flink.api.common.functions.FlatMapFunction
import org.apache.flink.api.common.typeinfo.BasicTypeInfo
import org.apache.flink.streaming.api.scala.{DataStream, StreamExecutionEnvironment}
import org.apache.flink.util.Collector

import scala.util.Try

object HelloFlink extends App {
  val env = StreamExecutionEnvironment.getExecutionEnvironment

  val text: DataStream[String] = env.socketTextStream("localhost", 9999)
  val splitter = (value: String) => value.toLowerCase.split("\\W+").filter((p: String) => p.nonEmpty)
  val result: DataStream[String] = text.flatMap(new FlatMapFunction[String, String] {
    override def flatMap(value: String, out: Collector[String]): Unit = splitter(value).foreach(f => out.collect(f))
  })(BasicTypeInfo.STRING_TYPE_INFO)
  result.print()

  // execute program
  try {
    env.execute("Flink Streaming Scala API Hello World")
  } catch {
    case e: Exception => println(s"Remember to run `nc -l 9999` in other terminal\n$e")
  }
}
