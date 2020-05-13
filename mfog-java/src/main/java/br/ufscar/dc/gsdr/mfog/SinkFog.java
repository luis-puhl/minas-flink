package br.ufscar.dc.gsdr.mfog;

import br.ufscar.dc.gsdr.mfog.structs.LabeledExample;
import br.ufscar.dc.gsdr.mfog.structs.Message;
import br.ufscar.dc.gsdr.mfog.structs.Serializers;
import br.ufscar.dc.gsdr.mfog.util.IdGenerator;
import br.ufscar.dc.gsdr.mfog.util.MfogManager;
import br.ufscar.dc.gsdr.mfog.util.TimeItConnection;
import com.esotericsoftware.kryonet.Connection;
import com.esotericsoftware.kryonet.Listener;
import com.esotericsoftware.kryonet.Server;
import org.slf4j.LoggerFactory;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.util.Comparator;
import java.util.Iterator;
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

        Server server = new Server() {
            protected Connection newConnection() {
                return new TimeItConnection();
            }
        };
        Serializers.registerMfogStructs(server.getKryo());
        server.addListener(new Listener() {
            @Override
            public void received(Connection conn, Object message) {
                TimeItConnection connection = (TimeItConnection) conn;
                if (message instanceof LabeledExample) {
                    LabeledExample prediction = (LabeledExample) message;
                    connection.items++;
                    predictions.add(new EvaluatorInput(prediction));
                    if (examples.size() == predictions.size()) {
                        log.info("received all items");
                        connection.sendTCP(new Message(Message.Intentions.DONE));
                        connection.close();
                    }
                }
                if (message instanceof Message) {
                    Message msg = (Message) message;
                    if (msg.isDone()) {
                        log.info("received Disconnect");
                        connection.close();
                    }
                    log.info("server received " + connection.finish());
                }
            }

            @Override
            public void disconnected(Connection conn) {
                TimeItConnection connection = (TimeItConnection) conn;
                log.info("disconnected " + connection.finish());
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
        long evaluationStart = System.currentTimeMillis();
        server.stop();
        server.close();
        final List<EvaluatorMatch> evaluatorMatches = new LinkedList<>();
        long matches = 0;
        long totalLoops = 0;
        predictions.sort((a, b) -> (int) (a.example.point.id - b.example.point.id));
        examples.sort((a, b) -> (int) (a.point.id - b.point.id));
        for (EvaluatorInput prediction : predictions) {
            for (Iterator<LabeledExample> iterator = examples.iterator(); iterator.hasNext(); ) {
                LabeledExample example = iterator.next();
                totalLoops++;
                if (prediction.example.point.id == example.point.id) {
                    EvaluatorMatch evaluatorMatch = new EvaluatorMatch(example, prediction);
                    if (evaluatorMatch.isMatch) {
                        matches++;
                    }
                    evaluatorMatches.add(evaluatorMatch);
                    iterator.remove();
                    break;
                }
            }
        }
        log.info("took me "+totalLoops+" loops " + (System.currentTimeMillis() - evaluationStart) + "ms");
        int items = evaluatorMatches.size();
        float misses = ((float) items - matches) / items;
        log.info("items=" + items + " matches=" + matches + " hits=" + (1 - misses) + " misses=" + misses);
        /*
         * -- make --
         * 13:28:07 INFO  mfog.SinkFog took me 653457 loops 489ms
         * 13:28:07 INFO  mfog.SinkFog items=653457 matches=206278 hits=0.31567186 misses=0.68432814
         * Program execution finished
         * Job with JobID 015e260e6d4ea8afa8bf30ec900fa8a0 has finished.
         * Job Runtime: 24579 ms
         *
         * -- make spot --
         * 13:16:46 INFO  mfog.SourceKyoto 653457 items, 14391 ms, 14390966805 ns, 45 i/ms
         * 13:16:46 INFO  mfog.Classifier Ran sink(string socket)  in 17.299s
         * 13:16:47 INFO  mfog.SinkFog took me 653457 loops 560ms
         * 13:16:47 INFO  mfog.SinkFog items=653457 matches=206278 hits=0.31567186 misses=0.68432814
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
