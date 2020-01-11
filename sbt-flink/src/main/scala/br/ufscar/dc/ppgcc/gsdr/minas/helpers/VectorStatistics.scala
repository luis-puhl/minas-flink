package br.ufscar.dc.ppgcc.gsdr.minas.helpers

object VectorStatistics {
  implicit class VectorStdDev(val items: Vector[Double]) extends AnyVal {
    def avg: Double = items.sum / items.size
    def variance: Double = {
      val avg = this.avg
      items.map(i => Math.pow(i - avg, 2)).sum
    }
    def stdDev: Double = Math.sqrt(variance)
  }
}
