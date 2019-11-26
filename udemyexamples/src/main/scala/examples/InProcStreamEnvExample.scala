package examples

import org.apache.flink.api.common.functions.FlatMapFunction
import org.apache.flink.streaming.api.scala.{DataStream, StreamExecutionEnvironment}
import org.apache.flink.util.Collector

object InProcStreamEnvExample extends App {
  // val env = ExecutionEnvironment.createLocalEnvironmentWithWebUI()
  // env.setParallelism(10)
  val env = StreamExecutionEnvironment.createLocalEnvironment(10)
  // val text: DataSet[String] = env.fromElements("di did ", "asasda", "asdf asdv asdv")
  val text: DataStream[String] = env.socketTextStream("localhost", 9999)
  // val mapper = (a: String, out: Collector[String]) => a.toLowerCase.split("\\W+").filter(p => p.nonEmpty).foreach(f => out.collect(f))
  val splitter = (value: String) => value.toLowerCase.split("\\W+").filter((p: String) => p.nonEmpty)
  // val result: DataStream[String] =  text.flatMap[String]((value: String, out: Collector[String]) => splitter(value).foreach((f: String) => out.collect(f)))
  val result: DataStream[String] = text.flatMap[String]((v: String, c: Collector[String]) => c.collect(v))
  result.print()
  env.execute(InProcStreamEnvExample.getClass.getName)
}
