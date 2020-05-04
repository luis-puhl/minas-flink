package br.ufscar.dc.gsdr.mfog.flink;

import br.ufscar.dc.gsdr.mfog.structs.Cluster;
import br.ufscar.dc.gsdr.mfog.structs.LabeledExample;
import br.ufscar.dc.gsdr.mfog.structs.Point;
import org.apache.flink.api.common.state.MapStateDescriptor;
import org.apache.flink.api.common.typeinfo.BasicTypeInfo;
import org.apache.flink.api.java.typeutils.ListTypeInfo;
import org.apache.flink.configuration.Configuration;
import org.apache.flink.streaming.api.functions.co.BroadcastProcessFunction;
import org.apache.flink.util.Collector;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.ArrayList;
import java.util.LinkedList;
import java.util.List;

public class MinasClassify extends BroadcastProcessFunction<Point, Cluster, LabeledExample> {
    protected static final Logger log = LoggerFactory.getLogger(MinasClassify.class);
    public static final MapStateDescriptor<String, List<Cluster>> modelStateDescriptor =
            new MapStateDescriptor<>("model", BasicTypeInfo.STRING_TYPE_INFO, new ListTypeInfo<>(Cluster.class));

    protected List<Point> buffer = new LinkedList<>();
    @Override
    public void open(Configuration parameters) throws Exception {
        super.open(parameters);
    }

    @Override
    public void processBroadcastElement(Cluster value, Context ctx, Collector<LabeledExample> out) throws Exception {
        List<Cluster> model = ctx.getBroadcastState(modelStateDescriptor).get("model");
        if (model == null) {
            model = new ArrayList<>(100);
        }
        model.add(value);
        ctx.getBroadcastState(modelStateDescriptor).put("model", model);
        for (Point example : buffer) {
            out.collect(classify(example, model));
        }
        buffer.clear();
        log.info("processBroadcastElement({})", model.size());
    }

    @Override
    public void processElement(Point value, ReadOnlyContext ctx, Collector<LabeledExample> out) throws Exception {
        log.info("processElement({})", value.id);
        List<Cluster> model = ctx.getBroadcastState(modelStateDescriptor).get("model");
        if (model != null) {
            out.collect(classify(value, model));
        } else {
            buffer.add(value);
            log.info("Buffering value: {}", buffer.size());
        }
    }

    LabeledExample classify(Point example, List<Cluster> model) {
        String minLabel = "no model";
        double minDist = Double.MIN_VALUE;
        for (Cluster cluster : model) {
            double distance = cluster.center.distance(example);
            if (distance < minDist) {
                minLabel = cluster.label;
                minDist = distance;
            }
        }
        return new LabeledExample(minLabel, example);
    }
}