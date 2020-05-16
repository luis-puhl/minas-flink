package br.ufscar.dc.gsdr.mfog;

import br.ufscar.dc.gsdr.mfog.structs.Message;
import br.ufscar.dc.gsdr.mfog.structs.Serializers;
import com.esotericsoftware.kryonet.Client;
import com.esotericsoftware.kryonet.util.TcpIdleSender;
import org.apache.flink.streaming.api.functions.sink.RichSinkFunction;
import org.apache.flink.streaming.api.functions.sink.SinkFunction;
import org.slf4j.LoggerFactory;

import java.util.LinkedList;
import java.util.Queue;

public class KryoNetClientSink<T> extends RichSinkFunction<T> {
    public final Class<T> generics;
    public final int port;
    public final String hostname;
    public final float idleThreshold;
    protected transient Client client;
    protected long sent;
    protected transient org.slf4j.Logger log;
    protected transient Queue<T> queue;

    public KryoNetClientSink(Class<T> generics, String hostname, int port, float idleThreshold) {
        this.generics = generics;
        this.port = port;
        this.hostname = hostname;
        this.idleThreshold = idleThreshold;
        setUpLogger();
    }

    void setUpLogger() {
        if (log == null) {
            log = LoggerFactory.getLogger(KryoNetClientSink.class);
        }
    }

    public void connect() throws Exception {
        client = new Client();
        Serializers.registerMfogStructs(client.getKryo());
        client.getKryo().register(Integer.class);
        client.start();
        client.connect(5000, hostname, port);
        client.sendTCP(new Message(Message.Intentions.SEND_ONLY));
        client.setIdleThreshold(this.idleThreshold);
        queue = new LinkedList<>();
    }

    @Override
    public void invoke(T value, SinkFunction.Context context) throws Exception {
        sent++;
        if (client == null || !client.isConnected()) {
            connect();
        }
        if (client.isIdle()) {
            client.sendTCP(value);
        } else {
            if (queue.isEmpty()) {
                // when Empty, this listener is removed
                client.addListener(new TcpIdleSender() {
                    @Override
                    protected Object next() {
                        if (queue.isEmpty()) {
                            return null;
                        } else {
                            return queue.poll();
                        }
                    }
                });
            }
            queue.add(value);
        }
    }

    @Override
    public void close() throws Exception {
        setUpLogger();
        log.warn("sent " + sent);
        if (client == null) return;
        client.sendTCP(new Message(Message.Intentions.DONE));
        client.stop();
    }
}