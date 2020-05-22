package br.ufscar.dc.gsdr.mfog;

import br.ufscar.dc.gsdr.mfog.structs.Message;
import br.ufscar.dc.gsdr.mfog.structs.Serializers;
import br.ufscar.dc.gsdr.mfog.util.TimeItConnection;
import com.esotericsoftware.kryonet.Connection;
import com.esotericsoftware.kryonet.Listener;
import com.esotericsoftware.kryonet.Server;
import org.apache.flink.streaming.api.functions.source.SourceFunction;
import org.slf4j.LoggerFactory;

import java.io.IOException;

public class KryoNetServerSource<T> implements SourceFunction<T> {
    static final org.slf4j.Logger log = LoggerFactory.getLogger(KryoNetServerSource.class);
    protected final Class<T> generics;
    protected final int port;
    protected Server server;

    public KryoNetServerSource(Class<T> generics, int port) {
        this.generics = generics;
        this.port = port;
    }

    @Override
    public void run(SourceContext<T> ctx) throws Exception {
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
        //
        server = new Server() {
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
                    if (msg.isDone()) {
                        log.info("done receiving");
                        conn.close();
                        return;
                    }
                    log.info("msg=" + msg);
                } else if (generics.isInstance(message)) {
                    lastUpdated.setNow();
                    connection.items++;
                    ctx.collect((T) message);
                }
            }

            @Override
            public void disconnected(Connection conn) {
                TimeItConnection connection = (TimeItConnection) conn;
                log.info(connection.finish());
            }
        });
        server.bind(port);
        server.run();
        /*
        server.start();
        final long duration = 30;
        final TimeUnit timeUnit = TimeUnit.SECONDS;
        final long totalTimeOut = timeUnit.toMillis(duration);
        while (true) {
            try {
                Thread.sleep(totalTimeOut);
            } catch (InterruptedException e) {
                break;
            }
            if (System.currentTimeMillis() - lastUpdated.get() > totalTimeOut) {
                log.warn("Timed-out EXIT");
                break;
            }
        }
         */
        log.info("done serving");
        cancel();
    }

    @Override
    public void cancel() {
        if (server == null) return;
        for (Connection connection : server.getConnections()) {
            connection.close();
        }
        server.stop();
        try {
            server.dispose();
        } catch (IOException e) {
            log.error("Failed to dispose.", e);
        }
        log.info("shutdown");
    }
}
