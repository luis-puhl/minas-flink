package br.ufscar.dc.gsdr.mfog;

import br.ufscar.dc.gsdr.mfog.structs.LabeledExample;
import br.ufscar.dc.gsdr.mfog.structs.Message;
import br.ufscar.dc.gsdr.mfog.structs.Serializers;
import br.ufscar.dc.gsdr.mfog.util.TimeItConnection;
import com.esotericsoftware.kryonet.Connection;
import com.esotericsoftware.kryonet.Listener;
import com.esotericsoftware.kryonet.Server;
import org.apache.flink.configuration.Configuration;
import org.apache.flink.streaming.api.functions.sink.RichSinkFunction;
import org.apache.flink.streaming.api.functions.sink.SinkFunction;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.util.LinkedList;
import java.util.Queue;

/**
 *
 * @param <T>
 */
public class KryoNetServerSink<T> extends RichSinkFunction<T> {
    static final org.slf4j.Logger log = LoggerFactory.getLogger(KryoNetServerSink.class);
    protected final Class<T> generics;
    protected final int port;
    protected Server server;
    protected final Queue<T> queue = new LinkedList<>();

    static class Timestamp {
        protected Long value;

        public synchronized Timestamp setNow() {
            this.value = System.currentTimeMillis();
            return this;
        }

        public synchronized long get() {
            return this.value;
        }
    }

    public KryoNetServerSink(Class<T> generics, int port) throws IOException {
        this.generics = generics;
        this.port = port;

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
            public void idle(Connection connection) {
                sender((TimeItConnection) connection);
            }

            @Override
            public void received(Connection conn, Object message) {
                log.info("msg=" + message);
            }

            @Override
            public void disconnected(Connection conn) {
                TimeItConnection connection = (TimeItConnection) conn;
                log.info(connection.finish());
            }
        });
        server.bind(port);
    }

    @Override
    public void open(Configuration parameters) throws Exception {
        super.open(parameters);
        server.start();
    }

    void sender(TimeItConnection connection) {
        while (connection.isIdle()) {
            T poll;
            synchronized (queue) {
                poll = queue.poll();
            }
            if (poll != null) {
                connection.sendTCP(poll);
            } else {
                break;
            }
        }
    }

    @Override
    public void invoke(T value, Context context) throws Exception {
        synchronized (queue) {
            queue.add(value);
        }
        for (Connection connection : server.getConnections()) {
            sender((TimeItConnection) connection);
        }
    }

    @Override
    public void close() throws Exception {
        super.close();
        for (Connection connection : server.getConnections()) {
            connection.sendTCP(new Message(Message.Intentions.DONE));
        }
        server.close();
    }
}
