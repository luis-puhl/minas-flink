package br.ufscar.dc.gsdr.mfog;

import br.ufscar.dc.gsdr.mfog.structs.LabeledExample;
import br.ufscar.dc.gsdr.mfog.structs.Message;
import br.ufscar.dc.gsdr.mfog.structs.Serializers;

import com.esotericsoftware.kryonet.Client;
import com.esotericsoftware.kryonet.Connection;
import com.esotericsoftware.kryonet.Listener;
import org.apache.flink.streaming.api.functions.source.SourceFunction;
import org.slf4j.LoggerFactory;

import java.io.IOException;

public class KryoNetClientSource<T> implements SourceFunction<T> {
    private final Class<T> generics;
    transient Client client;
    transient org.slf4j.Logger log;
    private int port;
    private String hostname;

    public KryoNetClientSource(Class<T> generics, String hostname, int port) {
        this.generics = generics;
        this.port = port;
        this.hostname = hostname;
        setUpLogger();
    }

    void setUpLogger() {
        if (log == null) {
            log = LoggerFactory.getLogger(KryoNetClientSource.class);
        }
    }

    @Override
    public void run(SourceContext<T> ctx) throws Exception {
        setUpLogger();
        client = new Client();
        Serializers.registerMfogStructs(client.getKryo());
        client.getKryo().register(Integer.class);
        client.addListener(new Listener() {
            int received;

            @SuppressWarnings("unchecked")
            @Override
            public void received(Connection connection, Object message) {
                super.received(connection, message);
                if (generics.isInstance(message)) {
                    received++;
                    ctx.collect((T) message);
                } else if (message instanceof Message) {
                    Message msg = (Message) message;
                    if (msg.isDone()) {
                        connection.close();
                        client.stop();
                        try {
                            client.dispose();
                        } catch (IOException e) {
                            log.error("Failed to dispose client.", e);
                        }
                    }
                    log.warn("received " + received);
                }
            }

            @Override
            public void disconnected(Connection connection) {
                super.disconnected(connection);
                log.info("client.stop");
                log.warn("received " + received);
                client.stop();
                try {
                    client.dispose();
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
        });
        client.start();
        client.connect(5000, hostname, port);
        client.sendTCP(new Message(Message.Intentions.CLASSIFIER));
        client.sendTCP(new Message(Message.Intentions.RECEIVE_ONLY));
        client.getUpdateThread().join();
        log.info("run is done");
        // client.run();  Client#update must be called in a separate thread during connect
    }

    @Override
    public void cancel() {
        setUpLogger();
        if (client == null) return;
        client.sendTCP(new Message(Message.Intentions.DONE));
        try {
            client.dispose();
        } catch (IOException e) {
            log.error("Failed to dispose client on cancel().", e);
        }
    }
}