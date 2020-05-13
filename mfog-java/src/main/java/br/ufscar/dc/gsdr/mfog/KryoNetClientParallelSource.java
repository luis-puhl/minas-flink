package br.ufscar.dc.gsdr.mfog;

import org.apache.flink.streaming.api.functions.source.ParallelSourceFunction;
import org.slf4j.LoggerFactory;

public class KryoNetClientParallelSource<T> extends KryoNetClientSource<T> implements ParallelSourceFunction<T> {
    public KryoNetClientParallelSource(Class<T> generics, String hostname, int port) {
        super(generics, hostname, port);
    }

    @Override
    protected void setUpLogger() {
        if (log == null) {
            log = LoggerFactory.getLogger(KryoNetClientParallelSource.class);
        }
    }
}