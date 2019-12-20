package br.ufscar.dc.ppgcc.gsdr.minas.datasets.kdd

/**
 *  0.0,2.6104176374007026E-7,0.0010571300219495107,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,
 *  0.015655577299412915,0.015655577299412915,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.03529411764705882,0.03529411764705882,
 *  1.0,0.0,0.11,0.0,0.0,0.0,0.0,0.0,normal
 *  35 fields
 */
case class KddCassalesEntry(
  f1: Double, f2: Double, f3: Double, f4: Double, f5: Double, f6: Double, f7: Double, f8: Double, f9: Double, f10: Double,
  f11: Double, f12: Double, f13: Double, f14: Double, f15: Double, f16: Double, f17: Double, f18: Double, f19: Double, f20: Double,
  f21: Double, f22: Double, f23: Double, f24: Double, f25: Double, f26: Double, f27: Double, f28: Double, f29: Double, f30: Double,
  f31: Double, f32: Double, f33: Double, f34: Double, label: String
) {
  def value: Vector[Double] = Vector(
    f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20,
    f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34
  )
}
