package br.ufscar.dc.ppgcc.gsdr.utils

import org.apache.flink.api.scala.DataSet

object FlinkUtils {

  implicit class RichSet[T](val value: DataSet[T]) extends AnyVal {
    def parallelism: Int = value.getParallelism
  }

}
