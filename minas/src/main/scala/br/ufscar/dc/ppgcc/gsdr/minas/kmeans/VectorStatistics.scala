package br.ufscar.dc.ppgcc.gsdr.minas.kmeans

object VectorStatistics {
  implicit class VectorStdDev(val items: Seq[Double]) extends AnyVal {
    def avg: Double = items.sum / items.size
    def variance: Double = {
      val avg = this.avg
      items.map(i => Math.pow(i - avg, 2)).sum
    }
    def variance(avg: Double): Double =
      items.map(i => Math.pow(i - avg, 2)).sum
    def stdDev: Double = Math.sqrt(variance)
    def stdDev(variance: Double): Double = Math.sqrt(variance)
    def statistics: (Double, Double, Double) = {
      val avg = this.avg
      val variance = this.variance(avg)
      val stdDev = this.stdDev(variance)
      (avg, variance, stdDev)
    }
    override def toString =
      this.statistics match {
        case (avg, _, stdDev) => f"($avg%1.3f +- $avg%1.3f)"
      }
  }
}