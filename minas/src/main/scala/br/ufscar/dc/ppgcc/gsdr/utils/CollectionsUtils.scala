package br.ufscar.dc.ppgcc.gsdr.utils

import scala.collection.AbstractIterator

object CollectionsUtils {

  implicit class RichIterator[A](val self: Iterator[A]) extends AnyVal {
    def zipWithLongIndex: Iterator[(A, Long)] = new AbstractIterator[(A, Long)] {
      var idx: Long = 0L

      def hasNext: Boolean = self.hasNext

      def next: (A, Long) = {
        val ret = (self.next(), idx)
        idx += 1
        ret
      }
    }
  }

}