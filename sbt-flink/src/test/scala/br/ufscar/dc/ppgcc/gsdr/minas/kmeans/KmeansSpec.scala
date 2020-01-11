package br.ufscar.dc.ppgcc.gsdr.minas.kmeans

import br.ufscar.dc.ppgcc.gsdr.minas.datasets.kdd.KddCassalesEntryFactory
import org.scalatest._
import org.scalatest.matchers.should.Matchers
import br.ufscar.dc.ppgcc.gsdr.minas.kmeans._

import scala.util.Try
// import com.opencsv._
import java.io._
import scala.collection.JavaConverters._
import scala.util._


class KmeansSpec extends FlatSpec with Matchers {
  val testSet = Vector(
    Point(0, Vector(0.0)),
    Point(1, Vector(1.0)),
    Point(2, Vector(3.0)),
    Point(3, Vector(7.0)),
    Point(4, Vector(8.0)),
    Point(5, Vector(9.0)),
    Point(6, Vector(10.0)),
    Point(7, Vector(12.0)),
    Point(8, Vector(12.5)),
    Point(9, Vector(12.7))
  )
  val initial = Vector[Cluster](
    Cluster(10, Point(10, Vector(4.0)), 0.0, "label"),
    Cluster(11, Point(11, Vector(9.0)), 0.0, "label"),
    Cluster(12, Point(12, Vector(13.0)), 0.0, "label")
  )
  def doTest = {
    val initialAssignedMap = Kmeans.groupByClosest(testSet, initial)
    val initialAssigned = Kmeans.updateClustersVariance(initialAssignedMap)
    val result = Kmeans.kmeans("Fixed values", testSet, initialAssigned)
    println(s"testSet = ${testSet.map(p => p.value.head)}")
    def printClusters(name: String, clusters: Vector[Cluster]) =
      println(s"$name = ${clusters.map(c => c.center.value.head)} ${clusters.map(c => c.variance).sum}")
    printClusters("initial", initialAssigned)
    printClusters("result", result)

    val rs = result.map(c => c.center.value.head)
    val predef = Vector(1.3333333333333333, 12.4, 8.5)
    val matches = rs.forall(p => predef.contains(p))
    matches should be (true)
  }

  // initialAssigned = Vector(1.3333333333333333, 12.4, 8.5) 7.0
  it should "Have fixed values" in doTest
  
  it should "Have fixed values using TaxiCabDistance" in {
    println("TaxiCabDistance")
    implicit val distance: Point.DistanceOperator = Point.TaxiCabDistance
    doTest
  }

  it should "Have fixed values using CosDistance" in {
    println("CosDistance")
    implicit val distance: Point.DistanceOperator = Point.CosDistance
    doTest
  }

  "When read from a file" should "initialize kmeans" in {
//    val inPathIni = "./tmpfs/KDDTe5Classes_fold1_ini.csv"
//    val outFilePath = "./tmpfs/out"
//    val bufferedSource = io.Source.fromFile(inPathIni)
//    val stream = bufferedSource.getLines
//      .map(line => KddCassalesEntryFactory.fromStringLine(line))
//      .zipWithIndex
//      .map(entry => (entry._1.label, Point(entry._2, entry._1.value)))
//      .toVector
//    val finalStream = stream.groupBy(p => p._1).mapValues(ps => {
//      val label = ps.head._1
//      val points = ps.map(p => p._2)
//      val clusters = Kmeans.kmeansInitialRandom(100, points)
//      (label, points, clusters)
//    })
//    writeTextFile(outFilePath.concat("/testing.txt"), finalStream.values.map(p => (p._1, p._3).toString()))
//    bufferedSource.close
  }
}
