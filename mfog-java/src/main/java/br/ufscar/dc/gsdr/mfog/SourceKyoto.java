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
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.stream.Collectors;
import java.util.stream.Stream;

public class SourceKyoto {
    static final org.slf4j.Logger log = LoggerFactory.getLogger(SourceKyoto.class);

    static class Sender<T> implements Runnable {
        final Iterator<T> examplesIter;
        final List<TimeItConnection> connections = new ArrayList<>(16);
        final org.slf4j.Logger log;

        Sender(Iterator<T> examplesIter, Class<T> typeInfo, Class<?> service) {
            String name = service.getName() + "-" + Sender.class.getName() + "-" + typeInfo.getName();
            log = LoggerFactory.getLogger(name);
            log.info(name);
            this.examplesIter = examplesIter;
        }

        synchronized int addConnection(TimeItConnection connection) {
            synchronized (connections) {
                connections.add(connection);
                return connections.size();
            }
        }

        @Override
        public void run() {
            // sender
            log.info("sending...");
            int activeConnections = 1;
            long sent = 0;
            while (examplesIter.hasNext() && activeConnections >= 1) {
                long thisSession = sent;
                synchronized (connections) {
                    for (Iterator<TimeItConnection> conns = connections.iterator(); conns.hasNext(); ) {
                        TimeItConnection current = conns.next();
                        if (!current.isConnected()) {
                            log.info("isConnected " + current);
                            conns.remove();
                            continue;
                        }
                        synchronized (examplesIter) {
                            if (examplesIter.hasNext()) {
                                // log.info("send " + current.getTcpWriteBufferSize());
                                current.items++;
                                sent++;
                                current.sendTCP(examplesIter.next());
                            } else {
                                break;
                            }
                        }
                    }
                    activeConnections = connections.size();
                }
                Thread.yield();
                if (thisSession == sent) {
                    log.info("All buffers full, nothing sent, will sleep. " + activeConnections);
                    try {
                        Thread.sleep(1000);
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }
                }
            }
            synchronized (connections) {
                for (Connection current : connections) {
                    current.sendTCP(new Message(Message.Intentions.DONE));
                    current.close();
                }
            }
            log.info("send...  ...done");
        }
    };

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
        class ParallelSenderConnection extends Connection {
            TimeIt timeIt = new TimeIt().start();
            int items = 0;
            Server server;
            ParallelSenderConnection(Server server) {
                this.server = server;
            }

            String finish() {
                return timeIt.finish(items);
            }
        }
        Server server = new Server() {
            protected Connection newConnection() {
                return new ParallelSenderConnection(this);
            }
        };
        Serializers.registerMfogStructs(server.getKryo());
        server.addListener(new Listener() {
            @Override
            public void received(Connection conn, Object message) {
                ParallelSenderConnection connection = (ParallelSenderConnection) conn;
                if (message instanceof Message) {
                    Message msg = (Message) message;
                    // log.info("msg=" + msg);
                    if (msg.isDone()) {
                        log.info("done");
                        connection.close();
                    }
                    if (msg.isClassifier()) {
                        log.info("Classifier is here");
                        Thread sender = new Thread(() -> {
                            log.info("sending... " + connection);
                            int strike = 3;
                            while (strike > 0 && connection.isConnected()) {
                                Thread.yield();
                                if (connection.getTcpWriteBufferSize() < 10) {
                                    strike--;
                                    try {
                                        Thread.sleep(10);
                                    } catch (InterruptedException e) {
                                        e.printStackTrace();
                                    }
                                    continue;
                                }
                                synchronized (examplesIterator) {
                                    if (examplesIterator.hasNext()) {
                                        connection.sendTCP(examplesIterator.next());
                                    } else {
                                        log.info("no more items \t" + connection);
                                        connection.sendTCP(new Message(Message.Intentions.DONE));
                                        connection.close();
                                        return;
                                    }
                                }
                            }
                            log.info("early disconnection \t" + connection);
                        }, "Sender for " + connection);
                        sender.start();
                    }
                }
            }

            @Override
            public void disconnected(Connection conn) {
                ParallelSenderConnection connection = (ParallelSenderConnection) conn;
                log.info(connection.finish());
            }
        });
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
