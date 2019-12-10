package examples.scala.KMeansVector

import org.apache.flink.streaming.api.scala.StreamExecutionEnvironment

object KMeansVector extends App {
  val env = StreamExecutionEnvironment.getExecutionEnvironment
  val csv$ = env.socketTextStream("localhost", 3232)
  CSV
  val csvTuple$ = csv$.map { line => line.}

  case class PacketDescriptor()
  val packtedDescriptor$ =
}
