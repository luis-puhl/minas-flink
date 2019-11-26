package examples

import org.apache.flink.api.common.functions.FlatMapFunction
import org.apache.flink.streaming.api.scala._
import org.apache.flink.streaming.api.windowing.time.Time
import org.apache.flink.util.Collector

object WindowWordCount extends App {
  val env = StreamExecutionEnvironment.getExecutionEnvironment
  val text = env.socketTextStream("localhost", 9999)

  val flatMapper = (a: String) => a.toLowerCase.split("\\W+").filter((p: String) => p.nonEmpty)
  val flatMapper2 = new FlatMapFunction[String, String] {
    override def flatMap(value: String, out: Collector[String]): Unit = {
      if (value != null)
       flatMapper(value).foreach(b => out.collect(b))
    }
  }
//  val flat = text.flatMap((value: String, out: Collector[String]) => flatMapper(value).foreach(b => out.collect(b)))
  val flat = text.flatMap((s: String, out: Collector[String]) => {
    if (s != null) {
      val d: Array[String] = s.toLowerCase.split("\\W+")
      println(d)
      if (d != null) {
        val f = d.filter((p: String) => true)
        f.foreach(g => out.collect(g))
      }
    }
  })
//  { _.toLowerCase.split("\\W+")filter { _.nonEmpty } }
  val map = flat.map { (_, 1) }
  val keyed = map.keyBy(0)
  val windw = keyed.timeWindow(Time.seconds(5))
  val counts = windw.sum(1)

  counts.print()

  env.execute("Window Stream WordCount")
}