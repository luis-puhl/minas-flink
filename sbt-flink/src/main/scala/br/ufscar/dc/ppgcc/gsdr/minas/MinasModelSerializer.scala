package br.ufscar.dc.ppgcc.gsdr.minas

import br.ufscar.dc.ppgcc.gsdr.minas.kmeans.{Cluster, Point}
import org.apache.flink.api.common.typeutils.{TypeSerializer, TypeSerializerSnapshot}
import org.apache.flink.api.scala.typeutils.CaseClassSerializer
import org.apache.flink.api.scala.typeutils.TraversableSerializer
import org.apache.flink.core.memory.{DataInputView, DataOutputView}
import scala.collection.JavaConversions
import scala.Function1

object MinasModelSerializer {
  val clazz: Class[MinasModel] = Class[MinasModel]
  val scalaFieldSerializers: Array[TypeSerializer[_]] = List(
    /* model: Vector[Cluster] */ TraversableSerializer.asInstanceOf[TypeSerializer],
    /* sleep: Vector[Cluster] */ TraversableSerializer.asInstanceOf[TypeSerializer],
    /* noMatch: Vector[Point] */ TraversableSerializer.asInstanceOf[TypeSerializer],
    /* config: Map[String, Int] */ TraversableSerializer.asInstanceOf[TypeSerializer],
    /* afterConsumedHook */ TypeSerializer[Null]
  ).toArray
}
class MinasModelSerializer extends CaseClassSerializer[MinasModel](MinasModelSerializer.clazz, MinasModelSerializer.scalaFieldSerializers) {
  override def snapshotConfiguration(): TypeSerializerSnapshot[MinasModel] = ???

  override def createInstance(fields: Array[AnyRef]): MinasModel = {
    type afterConsumedHookType = ((Option[String], Point, Cluster, Double)) => Unit
    MinasModel(
      model = fields(0).asInstanceOf[Vector[Cluster]],
      sleep = fields(0).asInstanceOf[Vector[Cluster]],
      noMatch = fields(0).asInstanceOf[Vector[Point]],
      config = fields(0).asInstanceOf[Map[String, Int]],
      afterConsumedHook = fields(0).asInstanceOf[afterConsumedHookType]
    )
  }
}
