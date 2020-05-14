package br.ufscar.dc.gsdr.mfog;

import br.ufscar.dc.gsdr.mfog.structs.LabeledExample;
import br.ufscar.dc.gsdr.mfog.structs.Message;
import br.ufscar.dc.gsdr.mfog.structs.Point;
import br.ufscar.dc.gsdr.mfog.structs.Serializers;
import br.ufscar.dc.gsdr.mfog.util.IdGenerator;
import br.ufscar.dc.gsdr.mfog.util.MfogManager;
import br.ufscar.dc.gsdr.mfog.util.TimeIt;
import br.ufscar.dc.gsdr.mfog.util.TimeItConnection;
import com.esotericsoftware.kryonet.*;
import org.slf4j.LoggerFactory;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.nio.channels.ServerSocketChannel;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.stream.Collectors;
import java.util.stream.Stream;

public class SourceKyoto {
    static final org.slf4j.Logger log = LoggerFactory.getLogger(SourceKyoto.class);

    public static void main(String[] args) throws IOException {
        IdGenerator idGenerator = new IdGenerator();
        List<Point> examples = new BufferedReader(
            new FileReader(MfogManager.Kyoto.basePath + MfogManager.Kyoto.test)).lines()
            // .limit(1000)
            .map(line -> LabeledExample.fromKyotoCSV(idGenerator.next(), line).point)
            .collect(Collectors.toList());

        class SynchronizedIterator<T> implements Iterator<T> {
            final Iterator<T> value;
            public SynchronizedIterator(Iterator<T> value) {
                this.value = value;
            }
            @Override
            public synchronized boolean hasNext() {
                return value.hasNext();
            }

            @Override
            public synchronized T next() {
                return value.next();
            }
        }
        final SynchronizedIterator<Point> examplesIterator = new SynchronizedIterator<>(examples.iterator());

        final int writeBufferSize = 70 * 1024 * 1024;
        Server server = new Server(writeBufferSize, 2048) {
            protected Connection newConnection() {
                return new TimeItConnection();
            }
        };
        Serializers.registerMfogStructs(server.getKryo());
        server.addListener(new Listener.ThreadedListener(new Listener() {
            @Override
            public void received(Connection conn, Object message) {
                TimeItConnection connection = (TimeItConnection) conn;
                if (message instanceof Message) {
                    Message msg = (Message) message;
                    // log.info("msg=" + msg);
                    if (msg.isDone()) {
                        log.info("done");
                        connection.close();
                    }
                    if (msg.isClassifier()) {
                        log.info("Classifier is here");
                        for (Point example : examples) {
                            connection.sendTCP(example);
                            connection.items++;
                        }
                        log.info("send...  ...done " + connection.finish());
                        connection.sendTCP(new Message(Message.Intentions.DONE));
                        connection.close();
                        // Thread sender = new Thread(() -> {
                        //     log.info("sending... " + connection);
                        //     while (connection.isConnected()) {
                        //         Thread.yield();
                        //         synchronized (examplesIterator) {
                        //             if (examplesIterator.hasNext()) {
                        //                 connection.sendTCP(examplesIterator.next());
                        //             } else {
                        //                 log.info("no more items \t" + connection);
                        //                 connection.sendTCP(new Message(Message.Intentions.DONE));
                        //                 connection.close();
                        //                 return;
                        //             }
                        //         }
                        //     }
                        //     log.info("early disconnection \t" + connection + " " + connection.finish());
                        // }, "Sender for " + connection);
                        // sender.start();
                    }
                }
            }

            @Override
            public void disconnected(Connection conn) {
                TimeItConnection connection = (TimeItConnection) conn;
                log.info(connection.finish());
            }
        }));
        server.bind(MfogManager.SOURCE_TEST_DATA_PORT);
        server.run();

        log.info("done");
    }

    void trainingSource() throws IOException {
        IdGenerator idGenerator = new IdGenerator();
        Stream<LabeledExample> labeledExampleStream = new BufferedReader(
            new FileReader(MfogManager.Kyoto.basePath + MfogManager.Kyoto.training)).lines()
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
                        for (; iterator.hasNext(); ) {
                            LabeledExample labeledExample = iterator.next();
                            labeledExample.point.time = System.currentTimeMillis();
                            connection.sendTCP(labeledExample);
                        }
                    }
                }
            }

            @Override
            public void disconnected(Connection conn) {
                TimeItConnection connection = (TimeItConnection) conn;
                log.info(connection.timeIt.finish(labeledExampleStream.count()));
            }
        });
        server.bind(MfogManager.SOURCE_TEST_DATA_PORT);
        server.run();
    }
}
