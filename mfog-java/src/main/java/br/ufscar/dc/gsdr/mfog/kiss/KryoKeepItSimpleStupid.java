package br.ufscar.dc.gsdr.mfog.kiss;

import br.ufscar.dc.gsdr.mfog.KryoNetClientParallelSource;
import br.ufscar.dc.gsdr.mfog.KryoNetClientSink;
import org.apache.flink.api.common.state.MapStateDescriptor;
import org.apache.flink.api.common.typeinfo.TypeHint;
import org.apache.flink.api.common.typeinfo.TypeInformation;
import org.apache.flink.streaming.api.datastream.BroadcastStream;
import org.apache.flink.streaming.api.datastream.DataStreamSink;
import org.apache.flink.streaming.api.datastream.DataStreamSource;
import org.apache.flink.streaming.api.datastream.SingleOutputStreamOperator;
import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment;
import org.apache.flink.streaming.api.functions.co.BroadcastProcessFunction;
import org.apache.flink.util.Collector;
import org.slf4j.LoggerFactory;

import java.util.*;

public class KryoKeepItSimpleStupid {
    static final org.slf4j.Logger log = LoggerFactory.getLogger(KryoKeepItSimpleStupid.class);
    static final float idle = 0.15f;
    static final int parallelism = 2;
    static final int waste_cpu = 0x02f0f0;
    // 4194304

    public static void main(String[] args) throws Exception {
        if (args.length > 0) {
            doServers();
        } else {
            doFlink();
        }
    }

    private static void doFlink() throws Exception {
        StreamExecutionEnvironment env = StreamExecutionEnvironment.getExecutionEnvironment();

        TypeInformation<String> stringTypeInfo = TypeInformation.of(String.class);
        DataStreamSource<String> sourceA = env.addSource(new KryoNetClientParallelSource<>(
            // new KryoNetClientSource<>(
            String.class, "localhost", 13000), "KryoNet A", stringTypeInfo);
        if (parallelism > 0) sourceA = sourceA.setParallelism(parallelism);

        DataStreamSource<String> sourceB = env.addSource(new KryoNetClientParallelSource<>(
            // new KryoNetClientSource<>(
            String.class, "localhost", 13001), "KryoNet B", stringTypeInfo);
        if (parallelism > 0) sourceB = sourceB.setParallelism(parallelism);

        //        sourceA.map(i -> i.concat("-flink"))
        //            .setParallelism(parallelism)
        //            .addSink(new KryoNetClientSink<>(String.class, "localhost", 13002)).name("KryoNet Sink A");
        //
        //        sourceB.map(i -> i.concat("-flink"))
        //            .setParallelism(parallelism)
        //            .addSink(new KryoNetClientSink<>(String.class, "localhost", 13003)).name("KryoNet Sink B");

        TypeInformation<List<String>> listTypeInfo = TypeInformation.of(new TypeHint<List<String>>() {
        });
        MapStateDescriptor<String, List<String>> stateDescriptor = new MapStateDescriptor<>("state", stringTypeInfo, listTypeInfo);
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
                    // waste time and CPU
                    StringBuilder sb = new StringBuilder();
                    for (int j = 0; j < waste_cpu; j++) {
                        sb.append("a");
                    }
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
            .name("Process Broadcast");
        if (parallelism > 0) process = process.setParallelism(parallelism);

        DataStreamSink<String> sink = process.addSink(new KryoNetClientSink<>(String.class, "localhost", 13004, idle))
            .name("KryoNet Sink processed");
        if (parallelism > 0) sink = sink.setParallelism(parallelism);

        env.execute("Keep It Simple, Stupid: Idle=" + idle + ", parallelism=" + parallelism + ", waste_cpu="+ Integer.toHexString(waste_cpu));
    }

    static Iterator<String> makeIterator(final int amount) {
        return new Iterator<String>() {
            int iterator = 0;

            @Override
            public boolean hasNext() {
                return iterator < amount;
            }

            @Override
            public synchronized String next() {
                return "0.0,0.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,0.0,1,0,0,0,0,0,0,0,0,1,A," + (iterator++);
            }
        };
    }

    static void doServers() throws InterruptedException {
        List<Thread> servers = Arrays.asList(
            //
            new Thread(new KryoKissServer<>(LoggerFactory.getLogger("Producer A"), 13000, makeIterator(700 * 1000), idle)),
            new Thread(new KryoKissServer<>(LoggerFactory.getLogger("Producer B"), 13001, makeIterator(100), idle)),
            // new Thread(new KissServer("Consumer A", 13002)),
            // new Thread(new KissServer("Consumer B", 13003)),
            new Thread(new KryoKissServer<>(LoggerFactory.getLogger("Consumer"), 13004, makeIterator(0), idle))
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
