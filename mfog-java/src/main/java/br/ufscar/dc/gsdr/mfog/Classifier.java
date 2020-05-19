/**
 * Copyright 2020 Luis Puhl
 * <p>
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * <p>
 * http://www.apache.org/licenses/LICENSE-2.0
 * <p>
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package br.ufscar.dc.gsdr.mfog;

import br.ufscar.dc.gsdr.mfog.structs.*;
import br.ufscar.dc.gsdr.mfog.util.MfogManager;
import com.esotericsoftware.kryonet.Client;
import com.esotericsoftware.kryonet.Connection;
import com.esotericsoftware.kryonet.Listener;
import org.apache.flink.api.common.ExecutionConfig;
import org.apache.flink.api.common.functions.RichMapFunction;
import org.apache.flink.api.common.serialization.SerializationSchema;
import org.apache.flink.api.common.state.ValueState;
import org.apache.flink.api.common.state.ValueStateDescriptor;
import org.apache.flink.api.common.typeinfo.TypeInformation;
import org.apache.flink.configuration.Configuration;
import org.apache.flink.streaming.api.datastream.DataStream;
import org.apache.flink.streaming.api.datastream.DataStreamSink;
import org.apache.flink.streaming.api.datastream.SingleOutputStreamOperator;
import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment;
import org.apache.flink.streaming.api.functions.co.CoProcessFunction;
import org.apache.flink.streaming.api.functions.sink.SinkFunction;
import org.apache.flink.streaming.api.functions.source.ParallelSourceFunction;
import org.apache.flink.util.Collector;
import org.slf4j.LoggerFactory;

import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.Serializable;
import java.util.ArrayList;
import java.util.LinkedList;
import java.util.List;
import java.util.Queue;

public class Classifier implements Serializable {
    static final org.slf4j.Logger log = LoggerFactory.getLogger(Classifier.class);
    static final float idle = 0.15f;

    public static class MinasClassify extends CoProcessFunction<Point, Model, LabeledExample> {
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

    public static class ModelAggregate extends RichMapFunction<Cluster, Model> {
        final org.slf4j.Logger log = LoggerFactory.getLogger(ModelAggregate.class);
        ValueState<Model> modelState;

        @Override
        public void open(Configuration parameters) throws Exception {
            super.open(parameters);
            modelState = getRuntimeContext().getState(new ValueStateDescriptor<Model>("model", Model.class));
            log.info("Map2Model Open");
        }

        @Override
        public Model map(Cluster value) throws Exception {
            Model model = modelState.value();
            if (model == null) {
                model = new Model();
                model.model = new ArrayList<>(100);
            }
            model.model.add(value);
            modelState.update(model);
            return model;
        }
    }

    public static class LabeledExampleSerializationSchema implements SerializationSchema<LabeledExample>, Serializable {
        static final org.slf4j.Logger log = LoggerFactory.getLogger(LabeledExampleSerializationSchema.class);
        protected transient ByteArrayOutputStream byteArrayOutputStream;
        protected transient DataOutputStream dataOutputStream;

        @Override
        public byte[] serialize(LabeledExample element) {
            if (byteArrayOutputStream == null) byteArrayOutputStream = new ByteArrayOutputStream(1024);
            if (dataOutputStream == null) dataOutputStream = new DataOutputStream(byteArrayOutputStream);
            try {
                element.write(dataOutputStream, element);
                dataOutputStream.flush();
            } catch (IOException e) {
                log.error("Failed to Flush", e);
            }
            byte[] bytes = byteArrayOutputStream.toByteArray();
            byteArrayOutputStream.reset();
            return bytes;
        }
    }

    public static void main(String[] args) throws Exception {
        StreamExecutionEnvironment env = StreamExecutionEnvironment.getExecutionEnvironment();
        ExecutionConfig config = env.getConfig();
        config.addDefaultKryoSerializer(Cluster.class, new Cluster());
        config.addDefaultKryoSerializer(Point.class, new Point());
        config.addDefaultKryoSerializer(LabeledExample.class, new LabeledExample());
        config.addDefaultKryoSerializer(Model.class, new Model());
        String hostname = MfogManager.SERVICES_HOSTNAME;
        String jobName = "Classifier ";

        // final int parallelism = env.getMaxParallelism();
        final int parallelism = 8;
        jobName += "parallelism("+parallelism+") ";

        DataStream<Cluster> modelSocket = env.addSource(
            new KryoNetClientSource<>(Cluster.class, hostname, MfogManager.MODEL_STORE_PORT), "KryoNet model",
            // getSource(Cluster.class, hostname, MfogManager.MODEL_STORE_PORT),
            TypeInformation.of(Cluster.class)
        );
        DataStream<Point> examples = env.addSource(
            new KryoNetClientSource<>(Point.class, hostname, MfogManager.SOURCE_TEST_DATA_PORT), "KryoNet examples",
            // getSource(hostname, MfogManager.SOURCE_TEST_DATA_PORT, Point.class),
            TypeInformation.of(Point.class)
        );
        SingleOutputStreamOperator<Model> model = modelSocket
            .keyBy(value -> 0)
            .map(new ModelAggregate())
            .setParallelism(1)
            .name("Map2Model");

        // ------------------------------
        examples = examples
            .rescale(); jobName += "rescale ";
            // .shuffle(); jobName += "shuffle ";

        SingleOutputStreamOperator<LabeledExample> out = examples
            .connect(model.broadcast()).process(new MinasClassify())
            .name("Classify");
        if (parallelism > 0) out = out.setParallelism(parallelism);

        DataStreamSink<LabeledExample> sink = out
            .addSink(new KryoNetClientSink<>(LabeledExample.class, hostname, MfogManager.SINK_MODULE_TEST_PORT, idle))
            // .addSink(getSink(hostname, MfogManager.SINK_MODULE_TEST_PORT))
            .name("KryoNet Sink");
        if (parallelism > 0) sink = sink.setParallelism(parallelism);
        jobName += "evaluate ";

        log.info("Ready to run baseline");
        long start = System.currentTimeMillis();
        env.execute(jobName);
        long elapsed = System.currentTimeMillis() - start;
        log.info("Ran " + jobName + " in " + elapsed * 10e-4 + "s");
    }

    static SinkFunction<LabeledExample> getSink(String hostname, int port) throws IOException {
        return new SinkFunction<LabeledExample>() {
            final Queue<LabeledExample> queue = new LinkedList<>();
            transient Client client;
            Client getConnection() throws IOException {
                if (client != null) return client;
                client = new Client();
                Serializers.registerMfogStructs(client.getKryo());
                client.addListener(new Listener() {
                    @Override
                    public void idle(Connection connection) {
                        while (connection.isIdle()) {
                            LabeledExample poll;
                            synchronized (queue) {
                                poll = queue.poll();
                            }
                            if (poll != null) {
                                connection.sendTCP(poll);
                            } else {
                                break;
                            }
                        }
                    }
                });
                client.start();
                client.connect(5000, hostname, port);
                client.sendTCP(new Message(Message.Intentions.SEND_ONLY));
                client.setIdleThreshold(idle);
                return client;
            }

            @Override
            public void invoke(LabeledExample value, Context context) throws Exception {
                getConnection();
                synchronized (queue) {
                    queue.add(value);
                }
            }
        };
    }

    static <T> ParallelSourceFunction<T> getSource(Class<T> tClass, String hostname, int port) throws IOException {
        return new ParallelSourceFunction<T>() {
            transient Client client;
            Client getConnection() throws IOException {
                if (client != null) return client;
                client = new Client();
                Serializers.registerMfogStructs(client.getKryo());
                client.getKryo().register(Integer.class);
                client.start();
                client.connect(5000, hostname, port);
                client.sendTCP(new Message(Message.Intentions.SEND_ONLY));
                client.setIdleThreshold(idle);
                return client;
            }
            @Override
            public void run(SourceContext<T> ctx) throws Exception {
                getConnection().addListener(new Listener() {
                    @Override
                    public void received(Connection connection, Object object) {
                        if (tClass.isInstance(object)) {
                            T next = (T) object;
                            ctx.collect(next);
                        }
                    }
                });
            }

            @Override
            public void cancel() {
                if (client != null) client.stop();
            }
        };
    }
}
