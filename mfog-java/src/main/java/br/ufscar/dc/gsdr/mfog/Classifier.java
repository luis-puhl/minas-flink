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

import br.ufscar.dc.gsdr.mfog.flink.SocketGenericStreamFunction;
import br.ufscar.dc.gsdr.mfog.structs.*;
import br.ufscar.dc.gsdr.mfog.util.Logger;
import br.ufscar.dc.gsdr.mfog.util.MfogManager;
import org.apache.flink.api.common.functions.AggregateFunction;
import org.apache.flink.api.common.functions.MapFunction;
import org.apache.flink.api.common.functions.RichMapFunction;
import org.apache.flink.api.common.state.ListState;
import org.apache.flink.api.common.state.ListStateDescriptor;
import org.apache.flink.api.common.state.ValueState;
import org.apache.flink.api.common.state.ValueStateDescriptor;
import org.apache.flink.api.common.typeinfo.TypeHint;
import org.apache.flink.api.common.typeinfo.TypeInformation;
import org.apache.flink.api.common.typeinfo.Types;
import org.apache.flink.api.java.tuple.Tuple2;
import org.apache.flink.configuration.Configuration;
import org.apache.flink.streaming.api.datastream.DataStream;
import org.apache.flink.streaming.api.datastream.DataStreamSource;
import org.apache.flink.streaming.api.datastream.SingleOutputStreamOperator;
import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment;
import org.apache.flink.streaming.api.functions.co.CoProcessFunction;
import org.apache.flink.streaming.api.functions.co.RichCoFlatMapFunction;
import org.apache.flink.streaming.api.functions.windowing.AllWindowFunction;
import org.apache.flink.streaming.api.windowing.time.Time;
import org.apache.flink.streaming.api.windowing.windows.GlobalWindow;
import org.apache.flink.util.Collector;
import org.slf4j.LoggerFactory;

import java.util.*;

public class Classifier {
    private static Logger LOG = Logger.getLogger(Classifier.class);

    public static void main(String[] args) throws Exception {
        StreamExecutionEnvironment env = StreamExecutionEnvironment.getExecutionEnvironment();
        env.getConfig().registerTypeWithKryoSerializer(Cluster.class, Serializers.ClusterSerializer.class);
        env.getConfig().registerTypeWithKryoSerializer(Point.class, Serializers.PointSerializer.class);
        env.getConfig().registerTypeWithKryoSerializer(LabeledExample.class, Serializers.LabeledExampleSerializer.class);
        env.getConfig().registerTypeWithKryoSerializer(Model.class, Serializers.ModelSerializer.class);

        DataStream<Cluster> modelSocket = env.addSource(
            new SocketGenericStreamFunction<>(
                MfogManager.SERVICES_HOSTNAME, MfogManager.MODEL_STORE_PORT, new Cluster(), Cluster.class, "Model Socket"
            ),
            "Model Socket",
            TypeInformation.of(Cluster.class)
        );
        DataStreamSource<Point> examples = env.addSource(
            new SocketGenericStreamFunction<>(
                MfogManager.SERVICES_HOSTNAME, MfogManager.SOURCE_TEST_DATA_PORT, new Point(), Point.class, "Examples Socket"
            ),
            "Examples Socket",
            TypeInformation.of(Point.class)
        );
         // modelSocket.print();
         // examples.print();

        /* ------------------------- */

        SingleOutputStreamOperator<Model> model = modelSocket.keyBy(value -> 0).map(new RichMapFunction<Cluster, Model>() {
            final org.slf4j.Logger log = LoggerFactory.getLogger(Classifier.class);
            ValueState<Model> modelState;

            @Override
            public void open(Configuration parameters) throws Exception {
                super.open(parameters);
                modelState = getRuntimeContext().getState(new ValueStateDescriptor<Model>("model", Model.class));
                log.info("Map2Model Open");
            }

            @Override
            public Model map(Cluster value) throws Exception {
                log.info("Map2Model map {}", value.id);
                Model model = modelState.value();
                if (model == null) {
                    model = new Model();
                    model.model = new ArrayList<>(100);
                }
                model.model.add(value);
                model.size++;
                modelState.update(model);
                return model;
            }
        }).name("Map2Model");
        // model.print();

        SingleOutputStreamOperator<LabeledExample> out = examples.rescale().connect(model.broadcast()).process(new CoProcessFunction<Point, Model, LabeledExample>() {
            final org.slf4j.Logger log = LoggerFactory.getLogger(Classifier.class);
            Model model;
            List<Point> buffer = new LinkedList<>();
            @Override
            public void processElement1(Point value, Context ctx, Collector<LabeledExample> out) {
                if (model == null) {
                    buffer.add(value);
                } else {
                    out.collect(model.classify(value));
                }
            }

            @Override
            public void processElement2(Model value, Context ctx, Collector<LabeledExample> out) {
                log.info("Classify map2 {}", value);
                // model.model.add(value);
                model = value;
                for (Point point : buffer) {
                    out.collect(model.classify(point));
                }
                buffer.clear();
            }
        })
        // .setParallelism(8) // locks the cluster in the creating state for all jobs
        .name("Classify");
        // out.print();
        TypeInformation<Map<String, Long>> info = TypeInformation.of(new TypeHint<Map<String, Long>>(){});
        //
        out.keyBy(v -> 0).map(
            new RichMapFunction<LabeledExample, Map<String, Long>>() {
                Map<String, Long> map = new HashMap<>();
                @Override
                public Map<String, Long> map(LabeledExample x) throws Exception {
                    long count = map.getOrDefault(x.label, 0L) + 1L;
                    map.put(x.label, count);
                    return map;
                }
            }
        ).print("aggregated");

        LOG.info("Ready to run baseline");
        long start = System.currentTimeMillis();
        env.execute("Classifier Baseline");
        long elapsed = System.currentTimeMillis() - start;
        LOG.info("Ran baseline in " + elapsed * 10e-4 + "s");
        LOG.info("done");
    }
}
