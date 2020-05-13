package br.ufscar.dc.gsdr.mfog;

import br.ufscar.dc.gsdr.mfog.structs.Cluster;
import br.ufscar.dc.gsdr.mfog.structs.Message;
import br.ufscar.dc.gsdr.mfog.structs.Serializers;
import br.ufscar.dc.gsdr.mfog.util.MfogManager;
import br.ufscar.dc.gsdr.mfog.util.TimeIt;
import br.ufscar.dc.gsdr.mfog.util.TimeItConnection;
import com.esotericsoftware.kryonet.Connection;
import com.esotericsoftware.kryonet.Listener;
import com.esotericsoftware.kryonet.Server;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

public class ModelStoreParallel {
    static final org.slf4j.Logger log = LoggerFactory.getLogger(ModelStoreParallel.class);

    public static void main(String[] args) throws IOException {
        final List<Cluster> store = new ArrayList<>(100);

        final List<Integer> index = new ArrayList<>(1);
        index.add(0);
        class ParallelSenderConnection extends TimeItConnection {
            void sender() {
                Thread sender = new Thread(() -> {
                    log.info("sending...");
                    while (isConnected()) {
                        synchronized (store) {
                            synchronized (index) {
                                Integer current = index.get(0);
                                if (current < store.size()) {
                                    sendTCP(store.get(current));
                                    index.set(0, current + 1);
                                }
                            }
                        }
                        Thread.yield();
                        try {
                            Thread.sleep(1000);
                        } catch (InterruptedException e) {
                            e.printStackTrace();
                        }
                    }
                });
                // sender.setDaemon(true);
                sender.start();
            }
        }
        //
        Server server = new Server() {
            protected Connection newConnection() {
                return new ParallelSenderConnection();
            }
        };
        Serializers.registerMfogStructs(server.getKryo());
        server.addListener(new Listener.ThreadedListener(new Listener() {
            @Override
            public void received(Connection conn, Object message) {
                ParallelSenderConnection connection = (ParallelSenderConnection) conn;
                if (message instanceof Message) {
                    Message msg = (Message) message;
                    log.info("msg=" + msg);
                    if (msg.isReceive()) {
                        log.info("Sending model");
                        connection.sender();
                    }
                    if (msg.isDone()) {
                        log.info("done");
                        conn.close();
                    }
                } else if (message instanceof Cluster) {
                    connection.items++;
                    synchronized (store) {
                        store.add((Cluster) message);
                    }
                }
            }

            @Override
            public void disconnected(Connection conn) {
                ParallelSenderConnection connection = (ParallelSenderConnection) conn;
                log.info(connection.finish());
            }
        }));
        server.bind(MfogManager.MODEL_STORE_PORT);
        server.run();
    }
}
