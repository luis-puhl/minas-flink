package br.ufscar.dc.gsdr.mfog.flink;

import br.ufscar.dc.gsdr.mfog.structs.LabeledExample;
import br.ufscar.dc.gsdr.mfog.structs.Model;
import br.ufscar.dc.gsdr.mfog.structs.Point;
import org.apache.flink.streaming.api.functions.co.CoProcessFunction;
import org.apache.flink.util.Collector;
import org.slf4j.LoggerFactory;

import java.util.LinkedList;
import java.util.List;

public class MinasClassify extends CoProcessFunction<Point, Model, LabeledExample> {
    final org.slf4j.Logger log = LoggerFactory.getLogger(MinasClassify.class);
    Model model;
    List<Point> buffer = new LinkedList<>();

    @Override
    public void processElement1(Point value, Context ctx, Collector<LabeledExample> out) {
        if (model == null) {
            buffer.add(value);
        } else {
            out.collect(model.classify(value));
            model.putToSleep();
        }
    }

    @Override
    public void processElement2(Model value, Context ctx, Collector<LabeledExample> out) {
        model = value;
        if (!buffer.isEmpty()) {
            log.info("Model update, cleaning buffer with {}", buffer.size());
            for (Point point : buffer) {
                out.collect(model.classify(point));
            }
            model.putToSleep();
            buffer.clear();
        }
    }
}
