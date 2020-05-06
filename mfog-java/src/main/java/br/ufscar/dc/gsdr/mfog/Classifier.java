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

import br.ufscar.dc.gsdr.mfog.flink.MinasClassify;
import br.ufscar.dc.gsdr.mfog.flink.ModelAggregate;
import br.ufscar.dc.gsdr.mfog.flink.SocketGenericSource;
import br.ufscar.dc.gsdr.mfog.structs.*;
import br.ufscar.dc.gsdr.mfog.util.Logger;
import br.ufscar.dc.gsdr.mfog.util.MfogManager;
import com.esotericsoftware.kryo.Kryo;
import com.esotericsoftware.kryo.io.Output;
import org.apache.flink.api.common.ExecutionConfig;
import org.apache.flink.api.common.functions.RichMapFunction;
import org.apache.flink.api.common.serialization.SerializationSchema;
import org.apache.flink.api.common.typeinfo.TypeInformation;
import org.apache.flink.streaming.api.datastream.DataStream;
import org.apache.flink.streaming.api.datastream.DataStreamSource;
import org.apache.flink.streaming.api.datastream.SingleOutputStreamOperator;
import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment;

import java.util.HashMap;
import java.util.Map;

public class Classifier {
    private static Logger LOG = Logger.getLogger(Classifier.class);

    public static void main(String[] args) throws Exception {
        StreamExecutionEnvironment env = StreamExecutionEnvironment.getExecutionEnvironment();
        ExecutionConfig config = env.getConfig();
        config.addDefaultKryoSerializer(Cluster.class, Serializers.ClusterSerializer.class);
        config.addDefaultKryoSerializer(Point.class, Serializers.PointSerializer.class);
        config.addDefaultKryoSerializer(LabeledExample.class, Serializers.LabeledExampleSerializer.class);
        config.addDefaultKryoSerializer(Model.class, Serializers.ModelSerializer.class);
        SocketGenericSource.DEFAULT_GZIP = MfogManager.USE_GZIP;

        DataStream<Cluster> modelSocket = env.addSource(
            new SocketGenericSource<>(MfogManager.SERVICES_HOSTNAME, MfogManager.MODEL_STORE_PORT,
                new Cluster(), Cluster.class, "Model Socket"
            ), "Model Socket", TypeInformation.of(Cluster.class));
        DataStreamSource<Point> examples = env.addSource(
            new SocketGenericSource<>(MfogManager.SERVICES_HOSTNAME, MfogManager.SOURCE_TEST_DATA_PORT,
                new Point(), Point.class, "Examples Socket"
            ), "Examples Socket", TypeInformation.of(Point.class));

        SingleOutputStreamOperator<Model> model = modelSocket.keyBy(value -> 0)
            .map(new ModelAggregate())
            .name("Map2Model");

        SingleOutputStreamOperator<LabeledExample> out = examples
            // .rescale()
            .connect(model.broadcast())
            .process(new MinasClassify())
            // .setParallelism(8) // locks the cluster in the creating state for all jobs
            .name("Classify");

        out.keyBy(v -> 0).map(new RichMapFunction<LabeledExample, Map<String, Long>>() {
            Map<String, Long> map = new HashMap<>();

            @Override
            public Map<String, Long> map(LabeledExample x) throws Exception {
                long count = map.getOrDefault(x.label, 0L) + 1L;
                map.put(x.label, count);
                return map;
            }
        }).print("aggregated");

        class LabeledExampleSerializationSchema implements SerializationSchema<LabeledExample> {
            transient Output o;
            transient Kryo k;
            transient Serializers.LabeledExampleSerializer s;
            @Override
            public byte[] serialize(LabeledExample element) {
                if (o == null) {
                    o = new Output(1024);
                    k = new Kryo();
                    s = new Serializers.LabeledExampleSerializer();
                }
                s.write(k, o, element);
                byte[] bytes = o.toBytes();
                o.clear();
                return bytes;
            }
        }
        out.writeToSocket(MfogManager.SERVICES_HOSTNAME, MfogManager.SINK_MODULE_TEST_PORT, new LabeledExampleSerializationSchema());

        LOG.info("Ready to run baseline");
        long start = System.currentTimeMillis();
        env.execute("Classifier Baseline");
        long elapsed = System.currentTimeMillis() - start;
        LOG.info("Ran baseline in " + elapsed * 10e-4 + "s");
        LOG.info("done");
    }
}
