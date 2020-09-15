package br.ufscar.dc.ppgcc.gsdr.utils

import org.apache.flink.api.scala.DataSet
import org.apache.flink.streaming.api.scala.DataStream

object FlinkUtils {

  implicit class RichSet[T](val value: DataSet[T]) extends AnyVal {
    def parallelism: Int = value.getParallelism
  }

}
