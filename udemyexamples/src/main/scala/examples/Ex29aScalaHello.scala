package examples

import org.apache.flink.core.fs.FileSystem
import org.apache.flink.streaming.api.scala._

object Ex29aScalaHello {
  def main(args: Array[String]) {
    // set up the streaming execution environment
    val env = StreamExecutionEnvironment.getExecutionEnvironment

    // get inputa data
    val data = env.readTextFile("courses")
    val courseLengths = data
      .map(x => x.toLowerCase.split(","))
      .map(x => (x(1).trim, x(2).trim.toInt))
      .keyBy(0)
      .sum(1)
    courseLengths.print()
    courseLengths.writeAsText("out", FileSystem.WriteMode.OVERWRITE)

    // execute program
    env.execute("Flink Streaming Scala API Hello World")
  }
}
