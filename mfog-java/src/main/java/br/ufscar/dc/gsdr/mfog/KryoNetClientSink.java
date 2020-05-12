package br.ufscar.dc.gsdr.mfog;

import br.ufscar.dc.gsdr.mfog.structs.Message;
import br.ufscar.dc.gsdr.mfog.structs.Serializers;
import com.esotericsoftware.kryonet.Client;
import org.apache.flink.streaming.api.functions.sink.RichSinkFunction;
import org.apache.flink.streaming.api.functions.sink.SinkFunction;
import org.slf4j.LoggerFactory;

public class KryoNetClientSink<T> extends RichSinkFunction<T> {
    private final Class<T> generics;
    protected int port;
    protected String hostname;
    transient Client client;
    long sent;
    transient org.slf4j.Logger log;

    public KryoNetClientSink(Class<T> generics, String hostname, int port) {
        this.generics = generics;
        this.port = port;
        this.hostname = hostname;
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
    }

    @Override
    public void invoke(T value, SinkFunction.Context context) throws Exception {
        sent++;
        if (client == null || !client.isConnected()) {
            connect();
        }
        client.sendTCP(value);
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