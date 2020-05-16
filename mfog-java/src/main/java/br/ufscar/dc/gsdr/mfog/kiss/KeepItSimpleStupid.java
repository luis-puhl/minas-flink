package br.ufscar.dc.gsdr.mfog.kiss;

import br.ufscar.dc.gsdr.mfog.KryoNetClientParallelSource;
import br.ufscar.dc.gsdr.mfog.KryoNetClientSink;
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

import java.util.Arrays;
import java.util.Collections;
import java.util.LinkedList;
import java.util.List;

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

        env.execute("Keep It Simple, Stupid( Idle=" + idle + ", parallelism=" + parallelism + ")");
    }

    static void doServers() throws InterruptedException {

        List<Thread> servers = Arrays.asList(
            //
            new Thread(new KissServer("Producer A", 13000, 700 * 1000, idle)),
            new Thread(new KissServer("Producer B", 13001, 100, idle)),
            // new Thread(new KissServer("Consumer A", 13002)),
            // new Thread(new KissServer("Consumer B", 13003)),
            new Thread(new KissServer("Consumer", 13004, 0, idle))
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
