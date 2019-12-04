package examples

import org.apache.flink.core.fs.FileSystem
import org.apache.flink.streaming.api.scala._

object Ex29bScalaHello extends App {
  val env = StreamExecutionEnvironment.createLocalEnvironment(10)

  val data = env.readTextFile("courses")
  val courseLengths = data
    .map(x => x.toLowerCase.split(","))
    .map(x => (x(1).trim, x(2).trim.toInt))
    .keyBy(0)
    .sum(1)
  courseLengths.print()
  courseLengths.writeAsText("local-out", FileSystem.WriteMode.OVERWRITE)

  env.execute("Flink Streaming Scala API Hello World v2")
}




//
//
//0.0,2.6104176374007026E-7,normal
//"" -> tuple -> trim -> window(1000) -> new Cluster() -> write.csv


















