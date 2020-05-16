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
    static final float idle = 0.10f;
    static final int parallelism = 1;

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
            String.class, "localhost", 13000), "KryoNet A", TypeInformation.of(String.class))
            .setParallelism(parallelism);

        DataStreamSource<String> sourceB = env.addSource(new KryoNetClientParallelSource<>(
            // new KryoNetClientSource<>(
            String.class, "localhost", 13001), "KryoNet B", TypeInformation.of(String.class))
            .setParallelism(parallelism);

        //        sourceA.map(i -> i.concat("-flink"))
        //            .setParallelism(parallelism)
        //            .addSink(new KryoNetClientSink<>(String.class, "localhost", 13002)).name("KryoNet Sink A");
        //
        //        sourceB.map(i -> i.concat("-flink"))
        //            .setParallelism(parallelism)
        //            .addSink(new KryoNetClientSink<>(String.class, "localhost", 13003)).name("KryoNet Sink B");

        MapStateDescriptor<String, List<String>> stateDescriptor = new MapStateDescriptor<>("state",
            TypeInformation.of(String.class), TypeInformation.of(new TypeHint<List<String>>() {
        })
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
                    out.collect(value + "," + state.size() + ',' + (Math.max(i, -1)));
                }

                @Override
                public void processBroadcastElement(String value, Context ctx, Collector<String> out) throws Exception {
                    List<String> state = ctx.getBroadcastState(stateDescriptor).get("state");
                    if (state == null) {
                        state = new LinkedList<>();
                    }
                    state.add(value);
                    state.sort(String::compareTo);
                    ctx.getBroadcastState(stateDescriptor).put("state", state);
                    for (String example : buffer) {
                        process(example, state, out);
                    }
                    buffer.clear();
                }
            })
            .setParallelism(parallelism)
            .name("Process Broadcast");

        process.addSink(new KryoNetClientSink<>(String.class, "localhost", 13004, idle))
            .name("KryoNet Sink processed")
            .setParallelism(parallelism);

        env.execute("Keep It Simple, Stupid( Idle="+idle+", parallelism=" + parallelism + ")");
    }

    static void doServers() throws InterruptedException {
        class KissServer extends Listener implements Runnable, Iterator<String> {
            final String name;
            final int port;
            final org.slf4j.Logger log;
            final int amount;
            Server server;
            int received = 0;
            Integer iterator = 0;
            long lastIdleMsg = System.currentTimeMillis();

            KissServer(String name, int port, int amount) {
                this.name = name;
                this.port = port;
                this.log = LoggerFactory.getLogger(name);
                this.amount = amount;
            }

            @Override
            public boolean hasNext() {
                return iterator < amount;
            }

            @Override
            public synchronized String next() {
                return "0.0,0.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,0.0,1,0,0,0,0,0,0,0,0,1,A," + (iterator++);
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
                synchronized (this) {
                    while (this.hasNext() && connection.isIdle()) {
                        connection.sendTCP(this.next());
                    }
                }
                if (!this.hasNext()) {
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
                if (this.hasNext()) return;
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
        List<Thread> servers = Arrays.asList(new Thread(new KissServer("Producer A", 13000, 700 * 1000)),
            new Thread(new KissServer("Producer B", 13001, 100)),
            // new Thread(new KissServer("Consumer A", 13002)),
            // new Thread(new KissServer("Consumer B", 13003)),
            new Thread(new KissServer("Consumer", 13004, 0))
        );
        for (Thread server : servers) {
            server.start();
        }
        for (Thread server : servers) {
            server.join();
            log.info("joined " + server);
        }
        System.exit(0);
    }
}
