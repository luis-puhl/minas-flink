package br.ufscar.dc.gsdr.mfog.util;

import java.util.Iterator;

public class IdGenerator implements Iterator<Integer> {
    int id = 0;

    @Override
    public boolean hasNext() {
        return true;
    }

    @Override
    public Integer next() {
        return id++;
    }
}