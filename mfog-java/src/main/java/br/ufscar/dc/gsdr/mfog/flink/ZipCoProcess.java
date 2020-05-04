package br.ufscar.dc.gsdr.mfog.flink;

import br.ufscar.dc.gsdr.mfog.structs.Cluster;
import br.ufscar.dc.gsdr.mfog.structs.LabeledExample;
import br.ufscar.dc.gsdr.mfog.structs.Point;
import org.apache.flink.api.common.state.MapState;
import org.apache.flink.api.common.state.MapStateDescriptor;
import org.apache.flink.api.common.typeinfo.BasicTypeInfo;
import org.apache.flink.api.common.typeinfo.TypeInformation;
import org.apache.flink.api.java.tuple.Tuple2;
import org.apache.flink.api.java.typeutils.ListTypeInfo;
import org.apache.flink.streaming.api.functions.co.BroadcastProcessFunction;
import org.apache.flink.util.Collector;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.List;
import java.util.Map;

public class ZipCoProcess<A, B> extends BroadcastProcessFunction<A, B, Tuple2<A, B>> {
    protected static final Logger LOG = LoggerFactory.getLogger(ZipCoProcess.class);
    public TypeInformation<Tuple2<A, B>> typeInformation;

    Class<A> aClass;
    Class<B> bClass;
    private final MapStateDescriptor<String, List<A>> mapStateDesc;
    private final MapStateDescriptor<String, B> ruleStateDescriptor;
    public ZipCoProcess(Class<A> aClass, Class<B> bClass) {
        this.aClass = aClass;
        this.bClass = bClass;
        this.mapStateDesc = new MapStateDescriptor<>("items", BasicTypeInfo.STRING_TYPE_INFO, new ListTypeInfo<>(aClass));
        this.ruleStateDescriptor = new MapStateDescriptor<>("model", BasicTypeInfo.STRING_TYPE_INFO, TypeInformation.of(bClass));
    }

    @Override
    public void processBroadcastElement(B value, Context ctx, Collector<Tuple2<A, B>> out) throws Exception {
        ctx.getBroadcastState(ruleStateDescriptor).put("model", value);
    }

    @Override
    public void processElement(A value, ReadOnlyContext ctx, Collector<Tuple2<A, B>> out) throws Exception {
        final MapState<String, List<A>> state = getRuntimeContext().getMapState(mapStateDesc);
        Iterable<Map.Entry<String, B>> modelMap = ctx.getBroadcastState(ruleStateDescriptor).immutableEntries();
        for (Map.Entry<String, B> entry: modelMap) {
            final String ruleName = entry.getKey();
            // final Rule rule = entry.getValue();
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