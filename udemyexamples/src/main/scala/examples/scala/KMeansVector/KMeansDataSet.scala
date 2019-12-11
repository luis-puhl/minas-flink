package examples.scala.KMeansVector

import java.nio.file.{Paths, Files}

import org.apache.flink.api.scala._
import org.apache.flink.core.fs.FileSystem
import org.apache.flink.streaming.api.scala.{DataStream, StreamExecutionEnvironment}

object KMeansDataSet extends App {
  val env = ExecutionEnvironment.getExecutionEnvironment
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

  val kddPath = "./kdd.ics.uci.edu/databases/kddcup99/kddcup.data"
  val tempPath = "./tmpfs/kddcup.data"
  val filePathInput = if ( Files.exists(Paths.get(tempPath)) ) tempPath else kddPath
  //
  val outFilePath = if ( Files.exists(Paths.get("tmpfs")) ) "./tmpfs/out" else "out"

  val kddString$: DataSet[String] = env.readTextFile(filePathInput)
  val words: DataSet[(String, String)] = kddString$.flatMap { (line, out) => {
    val values = line.split(",")
    symbolicIndexes.foreach((i: (Int, String)) => out.collect( (i._2, values(i._1)) ))
  }}
  type CountT = (String, Set[String])
  val count: DataSet[CountT] = words
    .map(t => (t._1, Set(t._2)))
    .groupBy(0)
    .reduce((value: CountT, acc: CountT) => (acc._1, acc._2 ++ value._2))
  count.writeAsText(outFilePath, FileSystem.WriteMode.OVERWRITE)
  env.execute()
}
