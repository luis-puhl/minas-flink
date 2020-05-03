package br.ufscar.dc.gsdr.mfog.flink;

import br.ufscar.dc.gsdr.mfog.structs.Cluster;
import br.ufscar.dc.gsdr.mfog.structs.LabeledExample;
import br.ufscar.dc.gsdr.mfog.structs.Point;
import org.apache.flink.streaming.api.functions.co.CoProcessFunction;
import org.apache.flink.util.Collector;

import java.util.List;

public class ClustersExamplesConnect extends CoProcessFunction<Point, List<Cluster>, LabeledExample> {
    @Override
    public void processElement1(Point value, Context ctx, Collector<LabeledExample> out) {
        long latency = System.currentTimeMillis() - value.time;
        LabeledExample example = new LabeledExample("latency=" + latency, value);
        out.collect(example);
    }

    @Override
    public void processElement2(List<Cluster> value, Context ctx, Collector<LabeledExample> out) {
        long latency = System.currentTimeMillis() - System.currentTimeMillis() - value.get(value.size() - 1).time;
        LabeledExample example = new LabeledExample("model latency=" + latency, Point.zero(22));
        out.collect(example);
    }
}