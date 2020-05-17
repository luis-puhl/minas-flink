package br.ufscar.dc.gsdr.mfog.kiss;

import br.ufscar.dc.gsdr.mfog.structs.Message;
import br.ufscar.dc.gsdr.mfog.structs.Serializers;
import br.ufscar.dc.gsdr.mfog.util.TimeItConnection;
import com.esotericsoftware.kryonet.Connection;
import com.esotericsoftware.kryonet.Listener;
import com.esotericsoftware.kryonet.Server;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.util.Iterator;

public class KissServer<T> extends Listener implements Runnable {
    final String name;
    final int port;
    final org.slf4j.Logger log;
    final float idle;
    Server server;
    int received = 0;
    final Iterator<T> iterator;
    long lastIdleMsg = System.currentTimeMillis();

    public KissServer(String name, int port, Iterator<T> iterator, float idle) {
        this.name = name;
        this.port = port;
        this.log = LoggerFactory.getLogger(name);
        this.iterator = iterator;
        this.idle = idle;
    }

    @Override
    public void run() {
        try {
            log.info("start");
            server = new Server() {
                protected Connection newConnection() {
                    return new TimeItConnection();
                }
            };
            Serializers.registerMfogStructs(server.getKryo());
            // server.addListener(this);
            server.addListener(new Listener.ThreadedListener(this));
            server.bind(port);
            server.run();
            server.stop();
            server.dispose();
            log.info("Done");
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    @Override
    public void connected(Connection connection) {
        log.info(" connected " + connection);
    }

    @Override
    public void idle(Connection conn) {
        TimeItConnection connection = (TimeItConnection) conn;
        long now = System.currentTimeMillis();
        if (now - lastIdleMsg > 1e3) {
            log.info(" idle " + connection.items);
            lastIdleMsg = now;
        }
        sender(connection);
    }

    void sender(TimeItConnection connection) {
        if (!connection.isSender) return;
        synchronized (iterator) {
            while (iterator.hasNext() && connection.isIdle()) {
                connection.sendTCP(iterator.next());
            }
        }
        if (!iterator.hasNext()) {
            connection.sendTCP(new Message(Message.Intentions.DONE));
            log.info(" sending...  ...done " + connection.finish());
            connection.isSender = false;
            connection.close();
        }
    }

    @Override
    public void received(Connection conn, Object message) {
        TimeItConnection connection = (TimeItConnection) conn;
        if (message instanceof Message) {
            Message msg = (Message) message;
            if (msg.isDone()) {
                log.info(" received Disconnect");
                connection.close();
                return;
            }
            if (msg.isReceive()) {
                log.info(" sending...");
                connection.setIdleThreshold(idle);
                connection.isSender = true;
                sender(connection);
                return;
            }
        }
        if (message instanceof String) {
            connection.items++;
            received++;
            if (connection.items % 1e5 == 0) {
                log.info((String) message);
            }
            return;
        }
        log.info(" received " + message);
    }

    @Override
    public void disconnected(Connection conn) {
        TimeItConnection connection = (TimeItConnection) conn;
        log.info(" disconnected " + connection.finish());
        if (iterator.hasNext()) return;
        try {
            Thread.sleep(100);
            if (server.getConnections().length == 0) {
                log.info(" No more connections total received=" + received);
                server.stop();
            }
        } catch (InterruptedException e) {
            //
        }
    }
}
