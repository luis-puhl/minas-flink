package br.ufscar.dc.gsdr.mfog.kiss;

import akka.serialization.Serializer;
import br.ufscar.dc.gsdr.mfog.KryoNetClientParallelSource;
import br.ufscar.dc.gsdr.mfog.KryoNetClientSink;
import br.ufscar.dc.gsdr.mfog.structs.Message;
import br.ufscar.dc.gsdr.mfog.structs.Serializers;
import br.ufscar.dc.gsdr.mfog.util.TimeItConnection;
import com.esotericsoftware.kryonet.Connection;
import com.esotericsoftware.kryonet.KryoSerialization;
import com.esotericsoftware.kryonet.Listener;
import com.esotericsoftware.kryonet.Server;
import com.esotericsoftware.kryonet.util.TcpIdleSender;
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

import java.io.Closeable;
import java.io.IOException;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.net.SocketException;
import java.nio.ByteBuffer;
import java.nio.channels.*;
import java.util.*;
import java.util.function.Consumer;

import static com.esotericsoftware.minlog.Log.*;
import static com.esotericsoftware.minlog.Log.debug;

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
            String.class, "localhost", 13000), "KryoNet A", TypeInformation.of(String.class))
            //.setParallelism(8)
            ;

        DataStreamSource<String> sourceB = env.addSource(new KryoNetClientParallelSource<>(
            // new KryoNetClientSource<>(
            String.class, "localhost", 13001), "KryoNet B", TypeInformation.of(String.class))
            //.setParallelism(8)
            ;

//        sourceA.map(i -> i.concat("-flink"))
//            .setParallelism(8)
//            .addSink(new KryoNetClientSink<>(String.class, "localhost", 13002)).name("KryoNet Sink A");
//
//        sourceB.map(i -> i.concat("-flink"))
//            .setParallelism(8)
//            .addSink(new KryoNetClientSink<>(String.class, "localhost", 13003)).name("KryoNet Sink B");

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
            .setParallelism(env.getMaxParallelism())
            .name("Process Broadcast");

        process.addSink(new KryoNetClientSink<>(String.class, "localhost", 13004)).name("KryoNet Sink processed");

        env.execute("Keep It Simple, Stupid setIdle .90, maxParallel");
    }

    static void doServers() throws InterruptedException {
        class KissServer extends Listener implements Runnable {
            final String name;
            final int port;
            Server server;
            final org.slf4j.Logger log;
            final int amount;

            KissServer(String name, int port, int amount) {
                this.name = name;
                this.port = port;
                this.log = LoggerFactory.getLogger(name);
                this.amount = amount;
            }

            @Override
            public void run() {
                try {
                    log.info(name + " start");
                    server = new Server() {
                        protected Connection newConnection() {
                            return new TimeItConnection();
                        }
                    };
                    Serializers.registerMfogStructs(server.getKryo());
                    server.getKryo().register(Integer.class);
                    server.addListener(new Listener.ThreadedListener(this));
                    server.bind(port);
                    server.run();
                    server.stop();
                    server.dispose();
                    log.info(name + " Done");
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }

            int i = -1;

            @Override
            public void connected(Connection connection) {
                log.warn(port + " connected " + connection);
            }

            long lastIdleMsg = System.currentTimeMillis();
            @Override
            public void idle(Connection conn) {
                TimeItConnection connection = (TimeItConnection) conn;
                long now = System.currentTimeMillis();
                if (i > 0 && now - lastIdleMsg > 1e3) {
                    log.info(port + " idle " + i);
                    lastIdleMsg = now;
                }
                sender(connection);
            }

            void sender(TimeItConnection connection) {
                while (i > 0 && connection.isSender && connection.isIdle()) {
                    // connection.items++;
                    connection.sendTCP("0.0,0.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,0.0,1,0,0,0,0,0,0,0,0,1,A," + (i--));
                    if (i == 0) {
                        i--;
                        connection.sendTCP(new Message(Message.Intentions.DONE));
                        log.info(port + " sending...  ...done " + connection.finish());
                        connection.isSender = false;
                        connection.close();
                        break;
                    }
                }
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
                        i = amount;
                        connection.setIdleThreshold(0.90f);
                        connection.isSender = true;
                        sender(connection);
                        return;
                    }
                }
                if (message instanceof String) {
                    connection.items++;
                    if (connection.items % 1e5 == 0) {
                        log.info((String) message);
                    }
                    return;
                }
                log.warn(port + " received " + message);
            }

            @Override
            public void disconnected(Connection conn) {
                TimeItConnection connection = (TimeItConnection) conn;
                log.warn(port + " disconnected " + connection.finish());
                if (i > 0) return;
                try {
                    Thread.sleep(100);
                    if (server.getConnections().length == 0) {
                        log.warn(port + " No more connections");
                        server.stop();
                    }
                } catch (InterruptedException e) {
                    //
                }
            }
        }
        List<Thread> servers = Arrays.asList(
            new Thread(new KissServer("Producer A", 13000, (int) (700 * 10e3))),
            new Thread(new KissServer("Producer B", 13001, 100)),
            // new Thread(new KissServer("Consumer A", 13002)),
            // new Thread(new KissServer("Consumer B", 13003)),
            new Thread(new KissServer("Consumer Broadcast", 13004, 10))
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

/*
ava -cp target/mfog-0.4.jar:../ref-git/flink-1.10.0/lib/* br.ufscar.dc.gsdr.mfog.kiss.KeepItSimpleStupid servers &
../ref-git/flink-1.10.0/bin/flink run --class br.ufscar.dc.gsdr.mfog.kiss.KeepItSimpleStupid target/mfog-0.4.jar
13:28:31 INFO  Producer B Producer B start
13:28:31 INFO  Consumer Broadcast Consumer Broadcast start
13:28:31 INFO  Producer A Producer A start
13:28:31 INFO  esotericsoftware.minlog kryonet: Server opened.
13:28:31 INFO  esotericsoftware.minlog kryonet: Server opened.
13:28:31 INFO  esotericsoftware.minlog kryonet: Server opened.
Job has been submitted with JobID 1f533c3c13a478da4a81a1704186e116
13:28:33 INFO  esotericsoftware.minlog kryonet: Connection 1 connected: /127.0.0.1
13:28:33 INFO  esotericsoftware.minlog kryonet: Connection 1 connected: /127.0.0.1
13:28:33 WARN  Producer A 13000 connected Connection 1
13:28:33 WARN  Producer B 13001 connected Connection 1
13:28:33 WARN  Producer A 13000 received Message{value=CLASSIFIER}
13:28:33 WARN  Producer A 13000 sending...
13:28:33 WARN  Producer B 13001 received Message{value=CLASSIFIER}
13:28:33 WARN  Producer B 13001 sending...
13:28:33 INFO  Producer B 13001 sending...  ...done Connection 1 102 items, 86 ms, 86189809 ns, 1 i/ms
13:28:33 INFO  esotericsoftware.minlog kryonet: Connection 1 disconnected.
13:28:33 WARN  Producer B 13001 disconnected Connection 1 102 items, 87 ms, 87540267 ns, 1 i/ms
13:28:33 INFO  esotericsoftware.minlog kryonet: Connection 1 connected: /127.0.0.1
13:28:33 WARN  Consumer Broadcast 13004 connected Connection 1
13:28:33 WARN  Consumer Broadcast 13004 received Message{value=SEND_ONLY}
13:28:33 WARN  Producer B 13001 No more connections
13:28:33 INFO  esotericsoftware.minlog kryonet: Server closed.
13:28:33 INFO  Producer B Producer B Done
13:28:35 INFO  Consumer Broadcast 0.0,0.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,0.0,1,0,0,0,0,0,0,0,0,1,A,6900002,100,-1
13:28:35 INFO  Producer A 13000 idle 6828154
13:28:36 INFO  Consumer Broadcast 0.0,0.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,0.0,1,0,0,0,0,0,0,0,0,1,A,6800002,100,-1
13:28:36 INFO  Producer A 13000 idle 6724389
13:28:37 INFO  Consumer Broadcast 0.0,0.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,0.0,1,0,0,0,0,0,0,0,0,1,A,6700002,100,-1
13:28:37 INFO  Producer A 13000 idle 6634667
13:28:38 INFO  Consumer Broadcast 0.0,0.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,0.0,1,0,0,0,0,0,0,0,0,1,A,6600002,100,-1
13:28:38 INFO  Producer A 13000 idle 6538704
13:28:39 INFO  Consumer Broadcast 0.0,0.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,0.0,1,0,0,0,0,0,0,0,0,1,A,6500002,100,-1
13:28:39 INFO  Producer A 13000 idle 6432988
13:28:40 INFO  Consumer Broadcast 0.0,0.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,0.0,1,0,0,0,0,0,0,0,0,1,A,6400002,100,-1
13:28:40 INFO  Producer A 13000 idle 6313619
13:28:41 INFO  Consumer Broadcast 0.0,0.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,0.0,1,0,0,0,0,0,0,0,0,1,A,6300003,100,-1
13:28:41 INFO  Producer A 13000 idle 6220776
13:28:42 INFO  Consumer Broadcast 0.0,0.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,0.0,1,0,0,0,0,0,0,0,0,1,A,6200003,100,-1
13:28:43 INFO  Producer A 13000 idle 6115450
13:28:43 INFO  Consumer Broadcast 0.0,0.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,0.0,1,0,0,0,0,0,0,0,0,1,A,6100003,100,-1
13:28:44 INFO  Producer A 13000 idle 6036261
13:28:44 INFO  Consumer Broadcast 0.0,0.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,0.0,1,0,0,0,0,0,0,0,0,1,A,6000003,100,-1
13:28:45 INFO  Producer A 13000 idle 5923133
13:28:45 INFO  esotericsoftware.minlog kryonet: Connection 1 disconnected.
13:28:45 INFO  esotericsoftware.minlog kryonet: Connection 1 disconnected.
13:28:45 WARN  Consumer Broadcast 13004 received Disconnect
13:28:45 WARN  Consumer Broadcast 13004 disconnected Connection 1 1087443 items, 12545 ms, 12544990982 ns, 86 i/ms
13:28:46 WARN  Consumer Broadcast 13004 No more connections
13:28:46 INFO  esotericsoftware.minlog kryonet: Server closed.
13:28:46 INFO  Consumer Broadcast Consumer Broadcast Done
Program execution finished
Job with JobID 1f533c3c13a478da4a81a1704186e116 has finished.
Job Runtime: 13049 ms

git_prompt_info:5: character not in range

 13:28:47  puhl@pokebola  ...project/minas-flink/mfog-java  ✹ ✚  29s 
$ 13:28:54 INFO  Producer A 13000 sending...  ...done Connection 1 7000002 items, 21296 ms, 21295661409 ns, 328 i/ms
13:28:54 WARN  Producer A 13000 disconnected Connection 1 7000002 items, 21296 ms, 21295881831 ns, 328 i/ms
13:28:54 WARN  Producer A 13000 No more connections
13:28:54 INFO  esotericsoftware.minlog kryonet: Server closed.
13:28:54 INFO  Producer A Producer A Done
13:28:54 INFO  kiss.KeepItSimpleStupid joined Thread[Thread-0,5,]
13:28:54 INFO  kiss.KeepItSimpleStupid joined Thread[Thread-1,5,]
13:28:54 INFO  kiss.KeepItSimpleStupid joined Thread[Thread-2,5,]

Job Name
Tasks
Keep It Simple, Stupid setIdle .90, maxParallel	2020-05-15 13:28:32	13s	2020-05-15 13:28:45
 */