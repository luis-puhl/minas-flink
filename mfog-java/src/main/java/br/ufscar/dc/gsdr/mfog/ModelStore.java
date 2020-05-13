package br.ufscar.dc.gsdr.mfog;

import br.ufscar.dc.gsdr.mfog.structs.Cluster;
import br.ufscar.dc.gsdr.mfog.structs.Message;
import br.ufscar.dc.gsdr.mfog.structs.Serializers;
import br.ufscar.dc.gsdr.mfog.util.MfogManager;
import br.ufscar.dc.gsdr.mfog.util.TimeItConnection;
import com.esotericsoftware.kryonet.Connection;
import com.esotericsoftware.kryonet.Listener;
import com.esotericsoftware.kryonet.Server;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.*;

public class ModelStore {
    static final org.slf4j.Logger log = LoggerFactory.getLogger(ModelStore.class);

    public static void main(String[] args) throws IOException {
        final List<Cluster> store = new ArrayList<>(100);
        //
        class Timestamp {
            protected Long value;
            public synchronized Timestamp setNow() {
                this.value = System.currentTimeMillis();
                return this;
            }
            public synchronized long get() {
                return this.value;
            }
        }
        Timestamp lastUpdated = new Timestamp().setNow();

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
                if (message instanceof Message) {
                    Message msg = (Message) message;
                    log.info("msg=" + msg);
                    if (msg.isReceive()) {
                        lastUpdated.setNow();
                        Thread sender = new Thread(() -> {
                            for (Cluster cluster : store) {
                                connection.sendTCP(cluster);
                                connection.items++;
                            }
                            connection.sendTCP(new Message(Message.Intentions.DONE));
                            connection.close();
                            lastUpdated.setNow();
                        }, "sender");
                        sender.setDaemon(true);
                        sender.start();
                    }
                    if (msg.isDone()) {
                        log.info("done receiving");
                        conn.close();
                    }
                } else if (message instanceof Cluster) {
                    lastUpdated.setNow();
                    connection.items++;
                    store.add((Cluster) message);
                }
            }

            @Override
            public void disconnected(Connection conn) {
                TimeItConnection connection = (TimeItConnection) conn;
                log.info(connection.finish());
            }
        });
        server.bind(MfogManager.MODEL_STORE_PORT);
        //
        // long running
        server.start();
        final long duration = 10;
        final TimeUnit timeUnit = TimeUnit.SECONDS;
        while (true) {
            try {
                Thread.sleep(timeUnit.toMillis(duration));
            } catch (InterruptedException e) {
                break;
            }
            if (System.currentTimeMillis() - lastUpdated.get() > timeUnit.toMillis(duration)) {
                log.warn("Timed-out EXIT");
                break;
            }
        }
        log.info("done serving");
        for (Connection connection : server.getConnections()) {
            connection.close();
        }
        server.stop();
        server.dispose();
        log.info("shutdown");
    }
}
