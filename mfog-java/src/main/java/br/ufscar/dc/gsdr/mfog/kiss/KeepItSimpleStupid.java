package br.ufscar.dc.gsdr.mfog.kiss;

import br.ufscar.dc.gsdr.mfog.KryoNetClientParallelSource;
import br.ufscar.dc.gsdr.mfog.KryoNetClientSink;
import br.ufscar.dc.gsdr.mfog.MinasClassify;
import br.ufscar.dc.gsdr.mfog.structs.Message;
import br.ufscar.dc.gsdr.mfog.structs.Serializers;
import br.ufscar.dc.gsdr.mfog.util.TimeItConnection;
import com.esotericsoftware.kryonet.Connection;
import com.esotericsoftware.kryonet.Listener;
import com.esotericsoftware.kryonet.Server;
import org.apache.flink.api.common.state.MapStateDescriptor;
import org.apache.flink.api.common.typeinfo.TypeInformation;
import org.apache.flink.streaming.api.datastream.BroadcastStream;
import org.apache.flink.streaming.api.datastream.DataStream;
import org.apache.flink.streaming.api.datastream.DataStreamSource;
import org.apache.flink.streaming.api.datastream.SingleOutputStreamOperator;
import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment;
import org.apache.flink.streaming.api.functions.co.BroadcastProcessFunction;
import org.apache.flink.streaming.api.functions.co.CoProcessFunction;
import org.apache.flink.util.Collector;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.sql.Time;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

public class KeepItSimpleStupid {
    static final org.slf4j.Logger log = LoggerFactory.getLogger(KeepItSimpleStupid.class);

    public static void main(String[] args) throws Exception {
        if (args.length > 0) {
            doServers();
        } else {
            doFlink();
        }
    }

    private static void doFlink() throws Exception {
        StreamExecutionEnvironment env = StreamExecutionEnvironment.getExecutionEnvironment();

        DataStreamSource<String> sourceA = env.addSource(new KryoNetClientParallelSource<>(
            // new KryoNetClientSource<>(
            String.class, "localhost", 13000), "KryoNet model", TypeInformation.of(String.class));

        DataStreamSource<String> sourceB = env.addSource(new KryoNetClientParallelSource<>(
            // new KryoNetClientSource<>(
            String.class, "localhost", 13001), "KryoNet model", TypeInformation.of(String.class));

        sourceA.map(i -> i.concat("-flink"))
            .addSink(new KryoNetClientSink<>(String.class, "localhost", 13002)).name("KryoNet Sink");

        sourceB.map(i -> i.concat("-flink"))
            .addSink(new KryoNetClientSink<>(String.class, "localhost", 13003)).name("KryoNet Sink");

        MapStateDescriptor<String, String> stateDescriptor = new MapStateDescriptor<>("state", String.class, String.class);
        BroadcastStream<String> broadcastB = sourceB.broadcast(stateDescriptor);
        SingleOutputStreamOperator<String> process = sourceA.connect(broadcastB)
            .process(new BroadcastProcessFunction<String, String, String>() {
                @Override
                public void processElement(String value, ReadOnlyContext ctx, Collector<String> out) throws Exception {
                    out.collect(value.concat(ctx.getBroadcastState(stateDescriptor).get("state")));
                }

                @Override
                public void processBroadcastElement(String value, Context ctx, Collector<String> out) throws Exception {
                    ctx.getBroadcastState(stateDescriptor).put("state", value);
                }
            });

        process.addSink(new KryoNetClientSink<>(String.class, "localhost", 13004)).name("KryoNet Sink");

        env.execute("Keep It Simple, Stupid");
    }

    static void doServers() {
        class Server implements Runnable {
            final String name;
            final int port;
            Server(String name, int port) {
                this.name = name;
                this.port = port;
            }

            @Override
            public void run() {
                try {
                    doServer(this.port, this.name);
                    log.info("Producer Done");
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
        }
        List<Server> servers = Arrays.asList(
            new Server("Producer A", 13000),
            new Server("Producer B", 13001),
            new Server("Consumer A", 13002),
            new Server("Consumer B", 13003),
            new Server("Consumer Broadcast", 13004)
        );
        for (Server server : servers) {
            new Thread(server).start();
        }
    }

    static void doServer(int port, String serverName) throws IOException {
        final org.slf4j.Logger log = LoggerFactory.getLogger(serverName);
        Server server = new Server() {
            protected Connection newConnection() {
                return new TimeItConnection();
            }
        };
        Serializers.registerMfogStructs(server.getKryo());
        server.getKryo().register(Integer.class);
        server.addListener(new Listener.ThreadedListener(new Listener() {
            @Override
            public void connected(Connection connection) {
                log.warn(port + " connected " + connection);
            }

            @Override
            public void received(Connection conn, Object message) {
                TimeItConnection connection = (TimeItConnection) conn;
                if (message instanceof Message) {
                    Message msg = (Message) message;
                    if (msg.isDone()) {
                        log.warn(port + " received Disconnect");
                        connection.close();
                    }
                    if (msg.isReceive()) {
                        log.warn(port + " sending...");
                        for (int i = 0; i < 100 * 1000; i++) {
                            connection.sendTCP(String.valueOf(i));
                            connection.items++;
                        }
                        connection.sendTCP(new Message(Message.Intentions.DONE));
                        log.warn(port + " sending...  ...done");
                    }
                    return;
                }
                if (message instanceof String) {
                    connection.items++;
                    return;
                }
                log.warn(port + " received " + message);
            }

            @Override
            public void disconnected(Connection conn) {
                TimeItConnection connection = (TimeItConnection) conn;
                log.warn(port + " disconnected " + connection.finish());
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
