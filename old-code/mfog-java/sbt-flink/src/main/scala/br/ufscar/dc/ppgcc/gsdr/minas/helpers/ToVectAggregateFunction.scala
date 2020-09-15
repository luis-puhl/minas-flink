package br.ufscar.dc.ppgcc.gsdr.minas.helpers

import br.ufscar.dc.ppgcc.gsdr.minas.kmeans._
import org.apache.flink.api.common.functions.AggregateFunction

import scala.collection._

class ToVectAggregateFunction extends AggregateFunction[(String, Point), Map[String, Vector[Point]], Map[String, Vector[Point]]] {
  override def createAccumulator(): Map[String, Vector[Point]] = Map()

  override def add(value: (String, Point), accumulator: Map[String, Vector[Point]]): Map[String, Vector[Point]] = {
    val current: Vector[Point] = accumulator.getOrElse[Vector[Point]](value._1, Vector[Point]())
    accumulator + (value._1 -> (current :+ value._2))
  }

  override def getResult(accumulator: Map[String, Vector[Point]]): Map[String, Vector[Point]] = accumulator

  override def merge(a: Map[String, Vector[Point]], b: Map[String, Vector[Point]]): Map[String, Vector[Point]] =
    a.keySet.++(b.keySet).map(key => {
      val aValues: Vector[Point] = a.getOrElse(key, Vector())
      val bValues: Vector[Point] = b.getOrElse(key, Vector())
      key -> aValues.++(bValues)
    }).toMap
}