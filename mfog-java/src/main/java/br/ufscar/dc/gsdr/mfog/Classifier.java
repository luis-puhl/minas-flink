/**
* Copyright 2020 Luis Puhl
* 
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
* 
*     http://www.apache.org/licenses/LICENSE-2.0
* 
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

package br.ufscar.dc.gsdr.mfog;

import br.ufscar.dc.gsdr.mfog.flink.AggregatorRichMap;
import br.ufscar.dc.gsdr.mfog.flink.SocketGenericStreamFunction;
import br.ufscar.dc.gsdr.mfog.structs.Cluster;
import br.ufscar.dc.gsdr.mfog.structs.LabeledExample;
import br.ufscar.dc.gsdr.mfog.structs.Point;
import br.ufscar.dc.gsdr.mfog.structs.Serializers;
import br.ufscar.dc.gsdr.mfog.util.Logger;
import br.ufscar.dc.gsdr.mfog.util.MfogManager;
import org.apache.flink.api.common.serialization.SerializationSchema;
import org.apache.flink.api.common.typeinfo.TypeInformation;
import org.apache.flink.streaming.api.datastream.DataStream;
import org.apache.flink.streaming.api.datastream.DataStreamSource;
import org.apache.flink.streaming.api.datastream.SingleOutputStreamOperator;
import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment;
import org.apache.flink.streaming.api.functions.co.CoProcessFunction;
import org.apache.flink.streaming.api.functions.source.SourceFunction;
import org.apache.flink.util.Collector;

import java.util.List;

public class Classifier {

    public static final int DELAY_BETWEEN_RETRIES = 1000;
    public static final int MAX_NUM_RETRIES = 10;

    public static void main(String[] args) throws Exception {
        new Classifier().baseline();
    }

    Logger LOG = Logger.getLogger(Classifier.class);

    void baseline() throws Exception {
        StreamExecutionEnvironment env = StreamExecutionEnvironment.getExecutionEnvironment();
        env.getConfig().registerTypeWithKryoSerializer(Cluster.class, Serializers.ClusterSerializer.class);
        env.getConfig().registerTypeWithKryoSerializer(Point.class, Serializers.PointSerializer.class);
        env.getConfig().registerTypeWithKryoSerializer(LabeledExample.class, Serializers.LabeledExampleSerializer.class);

        SourceFunction<Cluster> clusterSocket = new SocketGenericStreamFunction<>(
                MfogManager.SERVICES_HOSTNAME, MfogManager.MODEL_STORE_PORT,
                MAX_NUM_RETRIES, DELAY_BETWEEN_RETRIES,
                new Cluster(), false
        );
        DataStreamSource<Cluster> clusterSocketSource = env.addSource(
                clusterSocket,
                "model socket source",
                TypeInformation.of(Cluster.class)
        );
        DataStream<List<Cluster>> model = clusterSocketSource.map(new AggregatorRichMap<>()).broadcast();

        SourceFunction<Point> examplesSource = new SocketGenericStreamFunction<>(
                MfogManager.SERVICES_HOSTNAME, MfogManager.SOURCE_TEST_DATA_PORT,
                MAX_NUM_RETRIES, DELAY_BETWEEN_RETRIES,
                new Point(), false
        );
        DataStreamSource<Point> examples = env.addSource(
                examplesSource,
                "test examples source",
                TypeInformation.of(Point.class)
        );

        CoProcessFunction<Point, List<Cluster>, LabeledExample> merge = new CoProcessFunction<Point, List<Cluster>, LabeledExample>() {
            @Override
            public void processElement1(Point value, CoProcessFunction<Point, List<Cluster>, LabeledExample>.Context ctx, Collector<LabeledExample> out) {
                long latency = System.currentTimeMillis() - value.time;
                LabeledExample example = new LabeledExample("latency=" + latency, value);
                out.collect(example);
            }

            @Override
            public void processElement2(List<Cluster> value, CoProcessFunction<Point, List<Cluster>, LabeledExample>.Context ctx, Collector<LabeledExample> out) {
                long latency = System.currentTimeMillis() - System.currentTimeMillis() - value.get(value.size() - 1).time;
                LabeledExample example = new LabeledExample("model latency=" + latency, Point.zero(22));
                out.collect(example);
            }
        };

        SingleOutputStreamOperator<LabeledExample> out = examples
           // .keyBy(x -> x.id) // this keyby had no effect 183s vs 196s
           .connect(model)
           .process(merge);
        // SerializationSchema<LabeledExample> serializationSchema = element -> (element.json().toString() + "\n").getBytes();
        // out.writeToSocket(MfogManager.SERVICES_HOSTNAME, MfogManager.SINK_MODULE_TEST_PORT, serializationSchema);
        out.print();

        LOG.info("Ready to run baseline");
        long start = System.currentTimeMillis();
        env.execute("Classifier Baseline");
        long elapsed = System.currentTimeMillis() - start;
        LOG.info("Ran baseline in " + elapsed * 10e-4 + "s");
        LOG.info("done");
    }
}
