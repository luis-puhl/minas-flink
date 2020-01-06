package br.ufscar.dc.ppgcc.gsdr.minas.datasets.kdd

object KddCassalesEntryFactory {
  def empty: KddCassalesEntry = fromVectorValue(Vector.fill(34)(0.0d), "empty-value")

  def fromStringLine(line: String): KddCassalesEntry = {
    val items: List[String] = line.split(",").toList
    val label: String = items.last
    val value: List[Double] = items.slice(0, items.size - 1).map(i => i.toDouble)
    fromVectorValue(value, label)
  }

  def fromVectorValue(value: Seq[Double], label: String): KddCassalesEntry =
    KddCassalesEntry(
      value(0),
      value(1), value(2), value(3), value(4), value(5), value(6), value(7),
      value(8), value(9), value(10), value(11), value(12), value(13), value(14),
      value(15), value(16), value(17), value(18), value(19), value(20),
      value(21), value(22), value(23), value(24), value(25), value(26),
      value(27), value(28), value(29), value(30), value(31), value(32),
      value(33), label
      // value(34),
    )
}
