package br.ufscar.dc.gsdr.mfog;

import br.ufscar.dc.gsdr.mfog.structs.LabeledExample;
import br.ufscar.dc.gsdr.mfog.structs.Message;
import br.ufscar.dc.gsdr.mfog.structs.Serializers;

import br.ufscar.dc.gsdr.mfog.util.IdGenerator;
import br.ufscar.dc.gsdr.mfog.util.MfogManager;
import com.esotericsoftware.kryonet.Client;
import com.esotericsoftware.kryonet.Connection;
import com.esotericsoftware.kryonet.Listener;
import com.esotericsoftware.kryonet.Server;
import org.slf4j.LoggerFactory;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.util.LinkedList;
import java.util.List;
import java.util.stream.Collectors;

public class SinkFog {
    static final org.slf4j.Logger log = LoggerFactory.getLogger(SinkFog.class);

    public static void main(String[] args) throws IOException, InterruptedException {
        IdGenerator idGenerator = new IdGenerator();
        final List<LabeledExample> examples = new BufferedReader(
            new FileReader(MfogManager.Kyoto.basePath + MfogManager.Kyoto.test)).lines()
            // .limit(1000)
            .map(line -> LabeledExample.fromKyotoCSV(idGenerator.next(), line))
            .collect(Collectors.toCollection(LinkedList::new));
        //
        final List<EvaluatorInput> predictions = new LinkedList<>();
        //
        Server server = new Server();
        Serializers.registerMfogStructs(server.getKryo());
        server.addListener(new Listener() {
            int received;
            @Override
            public void received(Connection connection, Object message) {
                super.received(connection, message);
                if (message instanceof LabeledExample) {
                    LabeledExample prediction = (LabeledExample) message;
                    received++;
                    predictions.add(new EvaluatorInput(prediction));
                    if (examples.size() ==received) {
                        server.stop();
                    }
                }
                if (message instanceof Message) {
                    Message msg = (Message) message;
                    if (msg.isDone()) {
                        log.info("received Disconnect");
                        connection.close();
                    }
                    log.info("server received " + received);
                }
            }

            @Override
            public void disconnected(Connection connection) {
                super.disconnected(connection);
                log.info("disconnected " + connection);
                log.info("server received " + received);
            }
        });
        server.bind(MfogManager.SINK_MODULE_TEST_PORT);
        server.start();
        log.info("ready\n\n");
        while (predictions.size() == 0 || examples.size() != predictions.size()) {
            Thread.sleep(1000);
        }
        //
        log.info("everyone is here, time to match\n\n");

        final List<EvaluatorMatch> evaluatorMatches = new LinkedList<>();
        //
        long matches = 0;
        for (EvaluatorInput prediction : predictions) {
            for (LabeledExample example : examples) {
                if (prediction.example.point.id == example.point.id) {
                    EvaluatorMatch evaluatorMatch = new EvaluatorMatch(example, prediction);
                    if (evaluatorMatch.isMatch) {
                        matches++;
                    }
                    evaluatorMatches.add(evaluatorMatch);
                    examples.remove(example);
                    break;
                }
            }
        }
        int items = evaluatorMatches.size();
        float misses = ((float) items - matches) / items;
        log.info("items=" + items + " matches=" + matches + " hits=" + (1 - misses) + " misses=" + misses);
        server.stop();
        server.close();
        log.info("done");
        /*
         *
         * 02:13:57 INFO  taskmanager.Task Classify -> Sink: KryoNet Sink (8/8) (1f10678cc67825401ec2211d448381f3) switched from RUNNING to FINISHED.
         * 02:13:57 INFO  mfog.Classifier Ran sink(string socket)  in 13.548s
         * 02:13:57 INFO  akka.AkkaRpcService Stopped Akka RPC service.
         * 02:13:57  puhl@pokebola  ...project/minas-flink/mfog-java  ✖ ➜ ✹ ✚ ✭  26s 
         * 02:13:58 INFO  mfog.SinkFog everyone is here, time to match
         * 02:16:30 INFO  mfog.SinkFog items=653457 matches=206278 hits=0.31567186 misses=0.68432814
         * 02:16:30 INFO  mfog.SinkFog done
         */
    }
    static class EvaluatorInput {
        final long timeCreated, timeReceived;
        final LabeledExample example;
        public EvaluatorInput(LabeledExample example) {
            this.example = example;
            this.timeCreated = example.point.time;
            this.timeReceived = System.currentTimeMillis();
        }
    }
    static class EvaluatorMatch {
        final long timeCreated, timeReceived;
        final LabeledExample example;
        final EvaluatorInput prediction;
        final boolean isMatch;
        public EvaluatorMatch(LabeledExample example, EvaluatorInput prediction) {
            this.example = example;
            this.prediction = prediction;
            this.timeCreated = prediction.example.point.time;
            this.timeReceived = prediction.timeReceived;
            this.isMatch = example.label.equals(prediction.example.label);
        }
    }
}
