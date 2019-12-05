package examples.scala

import org.apache.flink.api.common.functions.MapFunction
import org.apache.flink.api.java.ExecutionEnvironment
import org.apache.flink.core.fs.FileSystem

object OfflineTraining extends App {
  val env = ExecutionEnvironment.getExecutionEnvironment
  val src = env.fromElements(1, 3, 4)
  val out = src.map(new MapFunction[Int, Int] {
    override def map(value: Int): Int = value * 2
  })
  out.writeAsText("out", FileSystem.WriteMode.OVERWRITE)
  env.execute("Minas/OfflineTraining")
}
