package examples.scala.KMeansVector

import java.nio.file.{Files, Paths}

import org.apache.flink.api.scala._
import org.apache.flink.core.fs.FileSystem
import org.apache.flink.streaming.api.scala.{DataStream, StreamExecutionEnvironment}

object KMeansVector extends App {
  val env = StreamExecutionEnvironment.getExecutionEnvironment
  
  val kddPath = "./kdd.ics.uci.edu/databases/kddcup99/kddcup.data"
  val tempPath = "./tmpfs/kddcup.data"
  val filePathInput = if ( Files.exists(Paths.get(tempPath)) ) tempPath else kddPath
  //
  val outFilePath = if ( Files.exists(Paths.get("tmpfs")) ) "./tmpfs/out" else "out"
  val kddString$: DataStream[String] = env.readTextFile(filePathInput)
  val symbolicIndex = Kdd.symbolicIndexes.keySet
  val indexMap = Kdd.symbolicIndexes.mapValues(x => Kdd.symbolicDictionaries(x).toIndexedSeq)
  //

  val item$: DataStream[Seq[Double]] = kddString$.flatMap { (line, out) =>
    val values = line.split(",")
    val valuesDouble: Seq[Double] = for {
      i <- 0 until values.length
    } yield if (symbolicIndex.contains(i)) indexMap(i).indexOf(values(i)).toDouble else values(i).toDouble
    out.collect(valuesDouble)
  }
  item$.writeAsText(outFilePath, FileSystem.WriteMode.OVERWRITE)
  env.execute()
}
