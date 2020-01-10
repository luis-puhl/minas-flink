package br.ufscar.dc.ppgcc.gsdr.minas.kmeans

import org.scalatest._
import org.scalatest.matchers.should.Matchers

import br.ufscar.dc.ppgcc.gsdr.minas.kmeans._

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
    Cluster(10, Point(10, Vector(4.0)), 0.0),
    Cluster(11, Point(11, Vector(9.0)), 0.0),
    Cluster(12, Point(12, Vector(13.0)), 0.0)
  )

  // initialAssigned = Vector(1.3333333333333333, 12.4, 8.5) 7.0
  it should "Have fixed values" in {
    val initialAssignedMap = Kmeans.groupByClosest(testSet, initial)
    val initialAssigned = Kmeans.updateClustersVariance(initialAssignedMap)
    val result = Kmeans.kmeans(testSet, initialAssigned)
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
}
