package br.ufscar.dc.gsdr.mfog;

import br.ufscar.dc.gsdr.mfog.structs.LabeledExample;
import br.ufscar.dc.gsdr.mfog.structs.Message;
import br.ufscar.dc.gsdr.mfog.structs.Serializers;
import br.ufscar.dc.gsdr.mfog.util.IdGenerator;

import br.ufscar.dc.gsdr.mfog.util.MfogManager;
import com.esotericsoftware.kryonet.Connection;
import com.esotericsoftware.kryonet.Listener;
import com.esotericsoftware.kryonet.Server;
import org.slf4j.LoggerFactory;

import java.io.*;
import java.util.Iterator;
import java.util.List;
import java.util.stream.Collectors;
import java.util.stream.Stream;

public class SourceKyoto {
    static final org.slf4j.Logger log = LoggerFactory.getLogger(SourceKyoto.class);

    public static void main(String[] args) throws InterruptedException, IOException {
        IdGenerator idGenerator = new IdGenerator();
        List<LabeledExample> examples = new BufferedReader(new FileReader(MfogManager.Kyoto.basePath + MfogManager.Kyoto.test)).lines()
            // .limit(1000)
            .map(line -> LabeledExample.fromKyotoCSV(idGenerator.next(), line)).collect(Collectors.toList());

        Server server = new Server(70 * 1024 * 1024, 2048) {
            protected Connection newConnection() {
                return new ModelStore.ModelConnection();
            }
        };
        Serializers.registerMfogStructs(server.getKryo());
        server.addListener(new Listener.ThreadedListener(new Listener() {
            Connection classifierConnection;
            Connection evaluatorConnection;
            @Override
            public void received(Connection connection, Object message) {
                if (message instanceof Message) {
                    Message msg = (Message) message;
                    if (msg.isDone()) {
                        log.info("done");
                        connection.close();
                    }
                    if (msg.isClassifier()) {
                        classifierConnection = connection;
                        log.info("Classifier is here");
                        for (LabeledExample labeledExample : examples) {
                            classifierConnection.sendTCP(labeledExample.point);
                        }
                        log.info("send...  ...done");
                        classifierConnection.sendTCP(new Message(Message.Intentions.DONE));
                        classifierConnection.close();
                    }
                }
            }

            @Override
            public void disconnected(Connection conn) {
                ModelStore.ModelConnection connection = (ModelStore.ModelConnection) conn;
                log.info(connection.timeIt.finish(examples.size()));
            }
        }));
        server.bind(MfogManager.SOURCE_TEST_DATA_PORT);
        server.start();

        SourceKyoto.log.info("done");
    }

    void trainingSource() throws IOException {
        IdGenerator idGenerator = new IdGenerator();
        Stream<LabeledExample> labeledExampleStream = new BufferedReader(new FileReader(MfogManager.Kyoto.basePath + MfogManager.Kyoto.training)).lines()
            .map(line -> LabeledExample.fromKyotoCSV(idGenerator.next(), line));
        Iterator<LabeledExample> iterator = labeledExampleStream.iterator();
        //
        Server server = new Server();
        Serializers.registerMfogStructs(server.getKryo());
        server.addListener(new Listener() {
            @Override
            public void received(Connection connection, Object message) {
                if (message instanceof Message) {
                    Message msg = (Message) message;
                    if (msg.isDone()) {
                        log.info("done");
                        connection.close();
                    }
                    if (msg.isReceive()) {
                        for (;iterator.hasNext();) {
                            LabeledExample labeledExample = iterator.next();
                            labeledExample.point.time = System.currentTimeMillis();
                            connection.sendTCP(labeledExample);
                        }
                    }
                }
            }

            @Override
            public void disconnected(Connection conn) {
                ModelStore.ModelConnection connection = (ModelStore.ModelConnection) conn;
                log.info(connection.timeIt.finish(labeledExampleStream.count()));
            }
        });
        server.bind(MfogManager.SOURCE_TEST_DATA_PORT);
        server.run();
    }
}
