package br.ufscar.dc.gsdr.mfog.util;

import com.esotericsoftware.kryonet.Connection;

public class TimeItConnection extends Connection {
    public TimeIt timeIt = new TimeIt().start();
    public long items = 0;

    public String finish() {
        return timeIt.finish(items);
    }
}