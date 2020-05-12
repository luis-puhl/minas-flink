package br.ufscar.dc.gsdr.mfog.kiss;

import br.ufscar.dc.gsdr.mfog.structs.Message;
import br.ufscar.dc.gsdr.mfog.structs.Serializers;

import com.esotericsoftware.kryonet.Connection;
import com.esotericsoftware.kryonet.Listener;
import com.esotericsoftware.kryonet.Server;
import org.slf4j.LoggerFactory;

import java.io.IOException;

public class KryoServer {
    static final org.slf4j.Logger log = LoggerFactory.getLogger(KryoServer.class);

    public static void main(String[] args) throws IOException {
        Thread a = new Thread(() -> {
            try {
                doServer(8888);
                log.info("Producer Done");
            } catch (IOException e) {
                e.printStackTrace();
            }
        });
        Thread b = new Thread(() -> {
            try {
                doServer(8080);
                log.info("Consumer Done");
            } catch (IOException e) {
                e.printStackTrace();
            }
        });
        a.start();
        b.start();
    }

    static void doServer(int port) throws IOException {
        Server server = new Server();
        Serializers.registerMfogStructs(server.getKryo());
        server.getKryo().register(Integer.class);
        server.addListener(new Listener.ThreadedListener(new Listener() {
            int i;

            @Override
            public void connected(Connection connection) {
                log.warn(port + " connected " + connection);
            }

            @Override
            public void received(Connection connection, Object message) {
                if (message instanceof Integer) {
                    i++;
                    return;
                }
                log.warn(port + " received " + message);
                if (message instanceof Message) {
                    Message msg = (Message) message;
                    if (msg.isDone()) {
                        log.warn(port + " received Disconnect");
                        connection.close();
                    }
                    if (msg.isReceive()) {
                        log.warn(port + " sending...");
                        for (int i = 0; i < 100; i++) {
                            connection.sendTCP(i);
                        }
                        connection.sendTCP(new Message(Message.Intentions.DONE));
                        log.warn(port + " sending...  ...done");
                    }
                }
            }

            @Override
            public void disconnected(Connection conn) {
                log.warn(port + " disconnected " + conn + " total=" + i);
                try {
                    Thread.sleep(1000);
                    if (server.getConnections().length == 0) {
                        log.warn(port + " No more connections");
                        server.close();
                    }
                } catch (InterruptedException e) {
                    //
                }
            }
        }));
        server.bind(port);
        server.run();
    }
}
