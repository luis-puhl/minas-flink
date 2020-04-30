package br.ufscar.dc.gsdr.mfog;

import br.ufscar.dc.gsdr.mfog.flink.AggregatorRichMap;
import br.ufscar.dc.gsdr.mfog.flink.SocketStreamFunction;
import br.ufscar.dc.gsdr.mfog.structs.Cluster;
import br.ufscar.dc.gsdr.mfog.structs.LabeledExample;
import br.ufscar.dc.gsdr.mfog.structs.Point;
import br.ufscar.dc.gsdr.mfog.util.Logger;
import br.ufscar.dc.gsdr.mfog.util.MfogManager;
import org.apache.flink.api.common.functions.MapFunction;
import org.apache.flink.api.common.functions.RichMapFunction;
import org.apache.flink.api.common.serialization.SerializationSchema;
import org.apache.flink.api.common.typeinfo.TypeInformation;
import org.apache.flink.configuration.Configuration;
import org.apache.flink.streaming.api.datastream.DataStream;
import org.apache.flink.streaming.api.datastream.DataStreamSource;
import org.apache.flink.streaming.api.datastream.SingleOutputStreamOperator;
import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment;
import org.apache.flink.streaming.api.functions.co.CoProcessFunction;
import org.apache.flink.streaming.api.functions.source.SourceFunction;
import org.apache.flink.util.Collector;

import java.util.ArrayList;
import java.util.List;

public class Classifier {
    static class ClustersExamplesConnect extends CoProcessFunction<Point, List<Cluster>, LabeledExample> {
        @Override
        public void processElement1(Point value, Context ctx, Collector<LabeledExample> out) {
            long latency = System.currentTimeMillis() - value.time;
            LabeledExample example = new LabeledExample("latency=" + latency, value);
            out.collect(example);
        }
        @Override
        public void processElement2(List<Cluster> value, Context ctx, Collector<LabeledExample> out) {
            long latency = System.currentTimeMillis() - System.currentTimeMillis() - value.get(value.size() -1).time;
            LabeledExample example = new LabeledExample("model latency=" + latency, Point.zero(22));
            out.collect(example);
        }
    }

    public static void main(String[] args) throws Exception {
        new Classifier().baseline();
    }

    Logger LOG = Logger.getLogger("Classifier");

    void baseline() throws Exception {
        StreamExecutionEnvironment env = StreamExecutionEnvironment.getExecutionEnvironment();

        SocketStreamFunction<Cluster> clusterSocket = new SocketStreamFunction<>(MfogManager.SERVICES_HOSTNAME, MfogManager.MODEL_STORE_PORT);
        DataStreamSource<Cluster> clusterSocketSource = env.addSource(
                clusterSocket,
                "clusterSocketSource",
                TypeInformation.of(Cluster.class)
        );
        DataStream<List<Cluster>> model = clusterSocketSource.map(
                new AggregatorRichMap<Cluster>()
        ).broadcast();

        /*
        DataStream<Point> examplesStringSource = env.addSource(
                new SocketStreamFunction<Point>(MfogManager.SERVICES_HOSTNAME, MfogManager.SOURCE_TEST_DATA_PORT)
        ).returns(Point.class);
        // examplesStringSource = env.socketTextStream(MfogManager.SERVICES_HOSTNAME, MfogManager.SOURCE_TEST_DATA_PORT).map((MapFunction<String, Point>) (value) -> Point.fromJson(value));
        SingleOutputStreamOperator<LabeledExample> out = examplesStringSource
           // .keyBy(x -> x.id) // this keyby had no effect 183s vs 196s
           .connect(model)
           .process(new ClustersExamplesConnect());
        SerializationSchema<LabeledExample> serializationSchema = element -> (element.json().toString() + "\n").getBytes();
        out.writeToSocket(MfogManager.SERVICES_HOSTNAME, MfogManager.SINK_MODULE_TEST_PORT, serializationSchema);
         */

        LOG.info("Ready to run baseline");
        long start = System.currentTimeMillis();
        env.execute("Classifier Baseline");
        long elapsed = System.currentTimeMillis() - start;
        LOG.info("Ran baseline in " + elapsed * 10e-4 + "s");
        LOG.info("done");
    }
}
