package br.ufscar.dc.ppgcc.gsdr.minas.helpers

import grizzled.slf4j.Logger
import org.apache.flink.api.common.state.{ValueState, ValueStateDescriptor}
import org.apache.flink.streaming.api.functions.KeyedProcessFunction
import org.apache.flink.util.Collector

class CountOrTimeoutWindow[K, T](count: Long, time: Long) extends KeyedProcessFunction[K, T, Vector[T]] {
  case class CountWithTimestamp(key: K, values: Vector[T], lastModified: Long)

  lazy val state: ValueState[CountWithTimestamp] = getRuntimeContext
    .getState(new ValueStateDescriptor[CountWithTimestamp]("CountWithTimeoutFunction", classOf[CountWithTimestamp]))
  lazy val LOG = Logger(getClass)

  override def processElement(value: T, ctx: KeyedProcessFunction[K, T, Vector[T]]#Context, out: Collector[Vector[T]]): Unit = {
    val timestamp: Long = {
      if (ctx != null && ctx.timestamp != null) ctx.timestamp
      else if (ctx != null && ctx.timerService != null && ctx.timerService.currentProcessingTime != null) ctx.timerService.currentProcessingTime
      else System.currentTimeMillis / 1000
    }
    ctx.timerService.registerEventTimeTimer(timestamp + time)
    ctx.timerService.registerProcessingTimeTimer(timestamp + time)
    val key = ctx.getCurrentKey
    //
    val current: CountWithTimestamp = Option(state.value) match {
      case Some(CountWithTimestamp(key, values, lastModified)) => {
        val items = values :+ value
        val remaining = if (items.size >= count) {
          LOG.info(s"by count flush out ${items.size}  $key")
          out.collect(items)
          Vector()
        } else items
        CountWithTimestamp(key, remaining, timestamp)
      }
      case None => CountWithTimestamp(key, Vector(value), timestamp)
    }
    state.update(current)
  }

  override def onTimer(timestamp: Long, ctx: KeyedProcessFunction[K, T, Vector[T]]#OnTimerContext, out: Collector[Vector[T]]): Unit = {
    // LOG.info(s"onTimer $timestamp")
    val current = state.value match {
      case CountWithTimestamp(key, items, lastModified) => {
        LOG.info(s"onTimer flush out ${items.size}  $key")
        if (timestamp == lastModified + time) {
          out.collect(items)
          CountWithTimestamp(key, Vector(), ctx.timestamp)
        } else state.value
      }
      case _ => {
        LOG.info(s"onTimer no match state ${state.value()}")
        state.value
      }
    }
    state.update(current)
  }
}