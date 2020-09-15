package br.ufscar.dc.ppgcc.gsdr.minas.kmeans

import java.io.{BufferedWriter, FileWriter}

import br.ufscar.dc.ppgcc.gsdr.minas.datasets.kdd.KddCassalesEntryFactory

import scala.util.{Failure, Try}

object KmeansTest extends App {

  def writeTextFile(fileName: String, values: Iterable[String]): Try[Unit] =
    Try(new BufferedWriter(new FileWriter(fileName))).flatMap((csvWriter: BufferedWriter) =>
      Try{
        values.foreach(p => csvWriter.write(p.toString.concat("\n")))
        csvWriter.close()
      } match {
        case f @ Failure(_) =>
          Try(csvWriter.close()).recoverWith{
            case _ => f
          }
        case success =>
          success
      }
    )

  val inPathIni = "./tmpfs/KDDTe5Classes_fold1_ini.csv"
  val outFilePath = "./tmpfs/out"
  val bufferedSource = io.Source.fromFile(inPathIni)
  val stream: Vector[(String, Point)] = bufferedSource.getLines
    .map(line => KddCassalesEntryFactory.fromStringLine(line))
    .zipWithIndex
    .map(entry => (entry._1.label, Point(entry._2, entry._1.value)))
    .toVector
  bufferedSource.close
  val finalStream: Map[String, (String, Vector[Point], Vector[Cluster])] = stream.groupBy(p => p._1).mapValues(group => {
    val label = group.head._1
    val points = group.map(p => p._2)
    println(s"Final Stream $label -> ${points.size}")
    val clusters = Kmeans.kmeansInitialRandom(label, 100, points)
    println("Kmeans.kmeansInitialRandom")
    (label, points, clusters)
  })
  //
  val asString: Iterable[String] = finalStream.values.flatMap(p => p._3.map(c => (p._1, c).toString()))
  writeTextFile(outFilePath.concat("/testing.txt"), asString)
  //
  val clusters = finalStream.mapValues(group => {
    val label: String = group._1
    val points: Vector[Point] = group._2
    val clusters: Vector[Cluster] = group._3
    (label, Kmeans.kmeans(label, points, clusters))
  }).values.flatMap(c => c._2.map(d => (c._1, d)))
  writeTextFile(outFilePath.concat("/testing-clusters.txt"), clusters.map(p => p.toString()))
}
