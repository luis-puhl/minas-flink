package br.ufscar.dc.gsdr.mfog.kiss;

import br.ufscar.dc.gsdr.mfog.KryoNetClientParallelSource;
import br.ufscar.dc.gsdr.mfog.KryoNetClientSink;
import br.ufscar.dc.gsdr.mfog.structs.Message;
import br.ufscar.dc.gsdr.mfog.structs.Serializers;
import br.ufscar.dc.gsdr.mfog.util.TimeItConnection;
import com.esotericsoftware.kryonet.Connection;
import com.esotericsoftware.kryonet.Listener;
import com.esotericsoftware.kryonet.Server;
import org.apache.flink.api.common.state.MapStateDescriptor;
import org.apache.flink.api.common.typeinfo.TypeHint;
import org.apache.flink.api.common.typeinfo.TypeInformation;
import org.apache.flink.streaming.api.datastream.BroadcastStream;
import org.apache.flink.streaming.api.datastream.DataStreamSource;
import org.apache.flink.streaming.api.datastream.SingleOutputStreamOperator;
import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment;
import org.apache.flink.streaming.api.functions.co.BroadcastProcessFunction;
import org.apache.flink.util.Collector;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.util.*;

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
            String.class, "localhost", 13000), "KryoNet A", TypeInformation.of(String.class));

        DataStreamSource<String> sourceB = env.addSource(new KryoNetClientParallelSource<>(
            // new KryoNetClientSource<>(
            String.class, "localhost", 13001), "KryoNet B", TypeInformation.of(String.class));

        sourceA.map(i -> i.concat("-flink"))
            .setParallelism(env.getMaxParallelism())
            .addSink(new KryoNetClientSink<>(String.class, "localhost", 13002)).name("KryoNet Sink A");

        sourceB.map(i -> i.concat("-flink"))
            .setParallelism(env.getMaxParallelism())
            .addSink(new KryoNetClientSink<>(String.class, "localhost", 13003)).name("KryoNet Sink B");

        MapStateDescriptor<String, List<String>> stateDescriptor = new MapStateDescriptor<>(
            "state",
            TypeInformation.of(String.class),
            TypeInformation.of(new TypeHint<List<String>>(){})
        );
        BroadcastStream<String> broadcastB = sourceB.broadcast(stateDescriptor);
        SingleOutputStreamOperator<String> process = sourceA.connect(broadcastB)
            .process(new BroadcastProcessFunction<String, String, String>() {
                List<String> buffer = new LinkedList<>();
                @Override
                public void processElement(String value, ReadOnlyContext ctx, Collector<String> out) throws Exception {
                    List<String> state = ctx.getBroadcastState(stateDescriptor).get("state");
                    if (state == null) {
                        buffer.add(value);
                    } else {
                        process(value, state, out);
                    }
                }

                void process(String value, List<String> state, Collector<String> out) {
                    int i = Collections.binarySearch(state, value);
                    out.collect(value + " -> " + i);
                }

                @Override
                public void processBroadcastElement(String value, Context ctx, Collector<String> out) throws Exception {
                    List<String> state = ctx.getBroadcastState(stateDescriptor).get("state");
                    if (state == null) {
                        state = new LinkedList<>();
                    }
                    state.add(value);
                    ctx.getBroadcastState(stateDescriptor).put("state", state);
                    for (String example : buffer) {
                        process(example, state, out);
                    }
                    buffer.clear();
                }
            })
            .setParallelism(env.getMaxParallelism())
            .name("Process Broadcast");

        process.addSink(new KryoNetClientSink<>(String.class, "localhost", 13004)).name("KryoNet Sink processed");

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
        /*
        Exception in thread "Thread-4" java.lang.OutOfMemoryError: Java heap space
            at java.nio.HeapByteBuffer.<init>(HeapByteBuffer.java:57)
            at java.nio.ByteBuffer.allocate(ByteBuffer.java:335)
            at com.esotericsoftware.kryonet.TcpConnection.<init>(TcpConnection.java:34)
        Server server = new Server(653457 * 1024, 2048) {
         */
        Server server = new Server(653457, 2048) {
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
                        return;
                    }
                    if (msg.isReceive()) {
                        log.warn(port + " sending...");
                        for (int i = 0; i < 653457; i++) {
                            // Exception in thread "pool-1-thread-1" com.esotericsoftware.kryo.KryoException: Buffer overflow. Available: 1, required: 2
                            // intermittent
                            connection.sendTCP(i + "0.0,0.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,0.0,1,0,0,0,0,0,0,0,0,1,A");
                            connection.items++;
                        }
                        connection.sendTCP(new Message(Message.Intentions.DONE));
                        log.warn(port + " sending...  ...done");
                        return;
                    }
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
        server.stop();
        server.dispose();
    }
}
