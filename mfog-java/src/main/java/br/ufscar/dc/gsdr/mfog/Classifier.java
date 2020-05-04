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
import org.apache.flink.api.common.functions.RichMapFunction;
import org.apache.flink.api.common.state.ListState;
import org.apache.flink.api.common.state.ListStateDescriptor;
import org.apache.flink.api.common.state.ValueState;
import org.apache.flink.api.common.state.ValueStateDescriptor;
import org.apache.flink.api.common.typeinfo.TypeInformation;
import org.apache.flink.configuration.Configuration;
import org.apache.flink.streaming.api.datastream.DataStream;
import org.apache.flink.streaming.api.datastream.DataStreamSource;
import org.apache.flink.streaming.api.datastream.SingleOutputStreamOperator;
import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment;
import org.apache.flink.streaming.api.functions.co.CoProcessFunction;
import org.apache.flink.streaming.api.functions.co.RichCoFlatMapFunction;
import org.apache.flink.util.Collector;
import org.slf4j.LoggerFactory;

import java.util.ArrayList;
import java.util.LinkedList;
import java.util.List;

public class Classifier {
    public static final int DELAY_BETWEEN_RETRIES = 1000;
    public static final int MAX_NUM_RETRIES = 10;
    private static Logger LOG = Logger.getLogger(Classifier.class);

    public static void main(String[] args) throws Exception {
        StreamExecutionEnvironment env = StreamExecutionEnvironment.getExecutionEnvironment();
        env.getConfig().registerTypeWithKryoSerializer(Cluster.class, Serializers.ClusterSerializer.class);
        env.getConfig().registerTypeWithKryoSerializer(Point.class, Serializers.PointSerializer.class);
        env.getConfig().registerTypeWithKryoSerializer(LabeledExample.class, Serializers.LabeledExampleSerializer.class);
        env.getConfig().registerTypeWithKryoSerializer(Model.class, Serializers.ModelSerializer.class);

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
        SingleOutputStreamOperator<Model> model1 = model.keyBy(c -> c.label).map(new RichMapFunction<Cluster, Model>() {
            ValueState<Model> modelState;
            final org.slf4j.Logger log = LoggerFactory.getLogger(CoProcessFunction.class);

            @Override
            public void open(Configuration parameters) throws Exception {
                super.open(parameters);
                modelState = getRuntimeContext().getState(new ValueStateDescriptor<Model>("model", Model.class));
            }

            @Override
            public Model map(Cluster value) throws Exception {
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
        }).setParallelism(8);

        SingleOutputStreamOperator<LabeledExample> out = examples.keyBy(Point::getId).connect(
                model.broadcast()
        ).process(new CoProcessFunction<Point, Cluster, LabeledExample>() {
            Model model = new Model();
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
                log.info("processElement1({}, model={})", value.id, model.size);
                buffer.add(value);
                if (model == null || model.model.isEmpty()) {
                    return;
                }
                for (Point example : buffer) {
                    out.collect(model.classify(example));
                }
            }

            @Override
            public void processElement2(Cluster value, Context ctx, Collector<LabeledExample> out) throws Exception {
                log.info("broadcast({}, model={})", value.id, model.size());
                model.model.add(value);
            }
        }).name("Zip");
        // out.filter(x -> x.point.id % 1000 == 0).print();

        SingleOutputStreamOperator<LabeledExample> out2 = examples.keyBy(Point::getId).connect(
                model1.broadcast()
        ).flatMap(new RichCoFlatMapFunction<Point, Model, LabeledExample>() {
            ValueState<Model> modelState;
            ListState<Point> buffer;
            @Override
            public void open(Configuration parameters) throws Exception {
                super.open(parameters);
                modelState = getRuntimeContext().getState(new ValueStateDescriptor<Model>("model", Model.class));
                buffer = getRuntimeContext().getListState(new ListStateDescriptor<Point>("buffer", Point.class));
            }
            @Override
            public void flatMap1(Point value, Collector<LabeledExample> out) throws Exception {
                Model model = modelState.value();
                if (model.isEmpty()) {
                    buffer.add(value);
                } else {
                    out.collect(model.classify(value));
                }
            }

            @Override
            public void flatMap2(Model value, Collector<LabeledExample> out) throws Exception {
                modelState.update(value);
                buffer.get().forEach(example -> out.collect(value.classify(example)));
                buffer.clear();
            }
        });
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
