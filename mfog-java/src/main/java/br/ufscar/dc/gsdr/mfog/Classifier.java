package br.ufscar.dc.gsdr.mfog;

import br.ufscar.dc.gsdr.mfog.structs.Cluster;
import br.ufscar.dc.gsdr.mfog.structs.Point;
import br.ufscar.dc.gsdr.mfog.util.MfogManager;
import br.ufscar.dc.gsdr.mfog.util.ModelStoreAkka;
import org.apache.flink.api.common.functions.MapFunction;
import org.apache.flink.api.common.functions.RichMapFunction;
import org.apache.flink.api.common.serialization.SerializationSchema;
import org.apache.flink.api.java.tuple.Tuple2;
import org.apache.flink.configuration.Configuration;
import org.apache.flink.streaming.api.datastream.DataStream;
import org.apache.flink.streaming.api.datastream.DataStreamSource;
import org.apache.flink.streaming.api.datastream.SingleOutputStreamOperator;
import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment;
import org.apache.flink.streaming.api.functions.co.CoProcessFunction;
import org.apache.flink.util.Collector;

import java.util.ArrayList;
import java.util.List;
import java.util.logging.Logger;

public class Classifier {
    static final Logger LOG = Logger.getLogger(ModelStoreAkka.class.getName());
    public static void main(String[] args) throws Exception {
        String jobName = ModelStoreAkka.class.getName();
        //

        baseline(jobName);
    }

    static class ModelString2ClustersMap extends RichMapFunction<String, List<Cluster>> {
        // new BroadcastProcessFunction[Point, Seq[Cluster], (Long, Long)]() {
        List<Cluster> model;
        @Override
        public List<Cluster> map(String value) {
            Cluster cl = Cluster.fromJson(value);
            model.add(cl);
            return model;
        }
        @Override
        public void open(Configuration parameters) throws Exception {
            super.open(parameters);
            model = new ArrayList<>(100);
        }
    }

    static class ClustersExamplesConnect extends CoProcessFunction<Point, List<Cluster>, Tuple2<Long, Long>> {
        @Override
        public void processElement1(Point value, Context ctx, Collector<Tuple2<Long, Long>> out) {
            out.collect(Tuple2.of(System.currentTimeMillis() - value.time, 0L));
        }
        @Override
        public void processElement2(List<Cluster> value, Context ctx, Collector<Tuple2<Long, Long>> out) {
            out.collect(Tuple2.of(0L, System.currentTimeMillis() - value.get(value.size() -1).time));
        }
    }

    static void baseline(String jobName) throws Exception {
        StreamExecutionEnvironment env = StreamExecutionEnvironment.getExecutionEnvironment();

        DataStreamSource<String> modelStringSource;
        modelStringSource = env.socketTextStream(MfogManager.SERVICES_HOSTNAME, MfogManager.MODEL_STORE_PORT);
        DataStream<List<Cluster>> clusters = modelStringSource.map(new ModelString2ClustersMap()).broadcast();

        DataStreamSource<String> examplesStringSource;
        examplesStringSource = env.socketTextStream(MfogManager.SERVICES_HOSTNAME, MfogManager.SOURCE_MODULE_TEST_PORT);
        SingleOutputStreamOperator<Tuple2<Long, Long>> out = examplesStringSource
           .map((MapFunction<String, Point>) Point::fromJson)
           .connect(clusters)
           .process(new ClustersExamplesConnect());
        //

        SerializationSchema<Tuple2<Long, Long>> serializationSchema = element -> element.toString().getBytes();
        out.writeToSocket(MfogManager.SERVICES_HOSTNAME, MfogManager.SINK_MODULE_TEST_PORT, serializationSchema);
        LOG.info("Ready to run baseline");
        long start = System.currentTimeMillis();
        env.execute(jobName);
        long elapsed = System.currentTimeMillis() - start;
        LOG.info("Ran baseline in " + elapsed * 10e-4 + "s");
    }
}
