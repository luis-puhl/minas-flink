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
import br.ufscar.dc.gsdr.mfog.flink.MinasClassify;
import br.ufscar.dc.gsdr.mfog.flink.SocketGenericStreamFunction;
import br.ufscar.dc.gsdr.mfog.flink.ZipCoProcess;
import br.ufscar.dc.gsdr.mfog.structs.*;
import br.ufscar.dc.gsdr.mfog.util.Logger;
import br.ufscar.dc.gsdr.mfog.util.MfogManager;
import org.apache.flink.api.common.functions.RichMapFunction;
import org.apache.flink.api.common.serialization.SerializationSchema;
import org.apache.flink.api.common.state.ListState;
import org.apache.flink.api.common.state.ListStateDescriptor;
import org.apache.flink.api.common.state.MapStateDescriptor;
import org.apache.flink.api.common.state.ValueStateDescriptor;
import org.apache.flink.api.common.typeinfo.BasicTypeInfo;
import org.apache.flink.api.common.typeinfo.TypeHint;
import org.apache.flink.api.common.typeinfo.TypeInformation;
import org.apache.flink.api.java.tuple.Tuple2;
import org.apache.flink.configuration.Configuration;
import org.apache.flink.streaming.api.datastream.BroadcastStream;
import org.apache.flink.streaming.api.datastream.DataStream;
import org.apache.flink.streaming.api.datastream.DataStreamSource;
import org.apache.flink.streaming.api.datastream.SingleOutputStreamOperator;
import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment;
import org.apache.flink.streaming.api.functions.co.CoProcessFunction;
import org.apache.flink.streaming.api.functions.source.SourceFunction;
import org.apache.flink.streaming.api.windowing.time.Time;
import org.apache.flink.util.Collector;
import org.slf4j.LoggerFactory;

import java.util.ArrayList;
import java.util.LinkedList;
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

        DataStream<Cluster> model = env.addSource(
                new SocketGenericStreamFunction<>(
                        MfogManager.SERVICES_HOSTNAME, MfogManager.MODEL_STORE_PORT,
                        MAX_NUM_RETRIES, DELAY_BETWEEN_RETRIES,
                        new Cluster(), false
                ),
                "Model Socket",
                TypeInformation.of(Cluster.class)
        );

        DataStreamSource<Point> examples = env.addSource(
                new SocketGenericStreamFunction<>(
                        MfogManager.SERVICES_HOSTNAME, MfogManager.SOURCE_TEST_DATA_PORT,
                        MAX_NUM_RETRIES, DELAY_BETWEEN_RETRIES,
                        new Point(), false
                ),
                "test examples source",
                TypeInformation.of(Point.class)
        );

        SingleOutputStreamOperator<LabeledExample> out = examples.keyBy(Point::getId).connect(
                model.broadcast()
        ).process(new CoProcessFunction<Point, Cluster, LabeledExample>() {
            List<Cluster> model = new ArrayList<>(100);
            List<Point> buffer = new LinkedList<>();
            final org.slf4j.Logger log = LoggerFactory.getLogger(CoProcessFunction.class);
            @Override
            public void open(Configuration parameters) throws Exception {
                super.open(parameters);
                // state = getRuntimeContext().getListState(new ListStateDescriptor<Cluster>("model", Cluster.class));
                // only on keyed streams, thus no parallelism.
            }

            @Override
            public void processElement1(Point value, Context ctx, Collector<LabeledExample> out) throws Exception {
                log.info("processElement1({}, model={})", value.id, model.size());
                buffer.add(value);
                if (model == null || model.isEmpty()) {
                    return;
                }
                for (Point example : buffer) {
                    String minLabel = "no model";
                    double minDist = Double.MAX_VALUE;
                    for (Cluster cluster : model) {
                        double distance = cluster.center.distance(example);
                        if (distance < minDist) {
                            minLabel = cluster.label;
                            minDist = distance;
                        }
                    }
                    out.collect(new LabeledExample(minLabel, example));
                }
            }

            @Override
            public void processElement2(Cluster value, Context ctx, Collector<LabeledExample> out) throws Exception {
                log.info("broadcast({}, model={})", value.id, model.size());
                model.add(value);
            }
        }).name("Zip");
        out.filter(x -> x.point.id % 1000 == 0).print();
        // SerializationSchema<LabeledExample> serializationSchema = element -> (element.json().toString() + "\n").getBytes();
        // out.writeToSocket(MfogManager.SERVICES_HOSTNAME, MfogManager.SINK_MODULE_TEST_PORT, serializationSchema);

        LOG.info("Ready to run baseline");
        long start = System.currentTimeMillis();
        env.execute("Classifier Baseline");
        long elapsed = System.currentTimeMillis() - start;
        LOG.info("Ran baseline in " + elapsed * 10e-4 + "s");
        LOG.info("done");
    }
}
