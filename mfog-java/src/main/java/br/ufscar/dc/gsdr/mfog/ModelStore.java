package br.ufscar.dc.gsdr.mfog;

import br.ufscar.dc.gsdr.mfog.structs.Cluster;
import br.ufscar.dc.gsdr.mfog.structs.Message;
import br.ufscar.dc.gsdr.mfog.structs.Serializers;

import br.ufscar.dc.gsdr.mfog.util.MfogManager;
import br.ufscar.dc.gsdr.mfog.util.TimeIt;
import com.esotericsoftware.kryonet.Connection;
import com.esotericsoftware.kryonet.Listener;
import com.esotericsoftware.kryonet.Server;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.util.LinkedList;
import java.util.List;

public class ModelStore {
    static final org.slf4j.Logger log = LoggerFactory.getLogger(ModelStore.class);

    public static void main(String[] args) throws IOException {
        List<Cluster> store = new LinkedList<>();

        Server server = new Server() {
            protected Connection newConnection() {
                return new ModelConnection();
            }
        };
        Serializers.registerMfogStructs(server.getKryo());
        server.addListener(new Listener() {
            @Override
            public void received(Connection conn, Object message) {
                ModelConnection connection = (ModelConnection) conn;
                if (message instanceof Message) {
                    Message msg = (Message) message;
                    if (msg.isReceive()) {
                        for (Cluster cluster : store) {
                            connection.sendTCP(cluster);
                        }
                        connection.sendTCP(new Message(Message.Intentions.DONE));
                        connection.close();
                        server.stop();
                    }
                    if (msg.isDone()) {
                        log.info("done");
                        conn.close();
                    }
                } else if (message instanceof Cluster) {
                    connection.rcv++;
                    store.add((Cluster) message);
                }
            }

            @Override
            public void disconnected(Connection conn) {
                ModelConnection connection = (ModelConnection) conn;
                log.info(connection.timeIt.finish(connection.rcv));
            }
        });
        server.bind(MfogManager.MODEL_STORE_PORT);
        server.run();
    }

    static class ModelConnection extends Connection {
        TimeIt timeIt = new TimeIt().start();
        long rcv = 0;
    }
}
