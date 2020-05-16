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

import br.ufscar.dc.gsdr.mfog.structs.Cluster;
import br.ufscar.dc.gsdr.mfog.structs.LabeledExample;
import br.ufscar.dc.gsdr.mfog.structs.Model;
import br.ufscar.dc.gsdr.mfog.structs.Point;
import br.ufscar.dc.gsdr.mfog.util.MfogManager;
import org.apache.flink.api.common.ExecutionConfig;
import org.apache.flink.api.common.typeinfo.TypeInformation;
import org.apache.flink.streaming.api.datastream.DataStreamSource;
import org.apache.flink.streaming.api.datastream.SingleOutputStreamOperator;
import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment;
import org.slf4j.LoggerFactory;

import java.io.Serializable;

public class Classifier implements Serializable {
    static final org.slf4j.Logger log = LoggerFactory.getLogger(Classifier.class);

    public static void main(String[] args) throws Exception {
        StreamExecutionEnvironment env = StreamExecutionEnvironment.getExecutionEnvironment();
        ExecutionConfig config = env.getConfig();
        config.addDefaultKryoSerializer(Cluster.class, new Cluster());
        config.addDefaultKryoSerializer(Point.class, new Point());
        config.addDefaultKryoSerializer(LabeledExample.class, new LabeledExample());
        config.addDefaultKryoSerializer(Model.class, new Model());
        String hostname = MfogManager.SERVICES_HOSTNAME;

        DataStreamSource<Cluster> modelSocket = env.addSource(
            new KryoNetClientSource<>(Cluster.class, hostname, MfogManager.MODEL_STORE_PORT), "KryoNet model",
            TypeInformation.of(Cluster.class)
        );
        DataStreamSource<Point> examples = env.addSource(
            new KryoNetClientSource<>(Point.class, hostname, MfogManager.SOURCE_TEST_DATA_PORT), "KryoNet examples",
            TypeInformation.of(Point.class)
        );
        SingleOutputStreamOperator<Model> model = modelSocket.keyBy(value -> 0)
            .map(new ModelAggregate())
            .setParallelism(1)
            .name("Map2Model");

        // ------------------------------
        String jobName = "" +
            //"rescale " + "shuffle " +
            // "parallelism(envMax) " +
            "sink(string socket) ";

        SingleOutputStreamOperator<LabeledExample> out = examples
            // .rescale()
            // .shuffle()
            .connect(model.broadcast()).process(new MinasClassify())
            // .setParallelism(env.getMaxParallelism()) // locks the cluster in the creating state for all jobs
            .name("Classify");

        out.addSink(new KryoNetClientSink<>(LabeledExample.class, hostname, MfogManager.SINK_MODULE_TEST_PORT, 0.9f))
            .name("KryoNet Sink");

        log.info("Ready to run baseline");
        long start = System.currentTimeMillis();
        env.execute(jobName);
        long elapsed = System.currentTimeMillis() - start;
        log.info("Ran " + jobName + " in " + elapsed * 10e-4 + "s");
    }
}
