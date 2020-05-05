package br.ufscar.dc.gsdr.mfog.flink;

import org.apache.flink.api.java.tuple.Tuple2;
import org.apache.flink.streaming.api.functions.co.CoProcessFunction;
import org.apache.flink.util.Collector;

public class ZipLatestCoPro<A, B> extends CoProcessFunction<A, B, Tuple2<A, B>> {
    A aLatest;
    B bLatest;

    @Override
    public void processElement1(A value, Context ctx, Collector<Tuple2<A, B>> out) throws Exception {
        aLatest = value;
        out.collect(Tuple2.of(aLatest, bLatest));
    }

    @Override
    public void processElement2(B value, Context ctx, Collector<Tuple2<A, B>> out) throws Exception {
        bLatest = value;
        out.collect(Tuple2.of(aLatest, bLatest));
    }
}
