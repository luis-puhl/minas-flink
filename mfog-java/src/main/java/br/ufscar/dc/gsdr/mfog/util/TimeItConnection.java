package br.ufscar.dc.gsdr.mfog.util;

import com.esotericsoftware.kryonet.Connection;

public class TimeItConnection extends Connection {
    public TimeIt timeIt = new TimeIt().start();
    public long items = 0;
    public boolean isSender;

    public String finish() {
        return this.toString() + " " + timeIt.finish(items);
    }

    @Override
    public int sendTCP (Object object) {
        items++;
        return super.sendTCP(object);
    }
}