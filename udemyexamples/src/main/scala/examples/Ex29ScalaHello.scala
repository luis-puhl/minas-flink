package examples

import org.apache.flink.api.common.io.InputFormat
import org.apache.flink.streaming.api.scala._

object Ex29ScalaHello {
  def main(args: Array[String]) {
    // set up the streaming execution environment
    val env = StreamExecutionEnvironment.getExecutionEnvironment

    // get inputa data
//    val data = env.readFile("/tmp/courses")
//    val courseLengths = data.map { _.toLowerCase.split("\\W+") }
//        .map { x => (x(2), x(1).toint) }
//        .groupBy(0)
//        .sum(1)
//
//    courseLengths.print()

    // execute program
//    env.execute("Flink Streaming Scala API Skeleton")
  }
}
