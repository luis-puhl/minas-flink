package examples;

import org.apache.flink.api.java.tuple.Tuple3;
import org.apache.flink.api.java.utils.ParameterTool;
import org.apache.flink.core.fs.FileSystem;
import org.apache.flink.streaming.api.datastream.DataStream;
import org.apache.flink.streaming.api.datastream.SingleOutputStreamOperator;
import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment;
import org.apache.flink.streaming.api.functions.ProcessFunction;
import org.apache.flink.util.Collector;
import org.apache.flink.util.OutputTag;

public class Ex26Split {
    public static void main(String[] args) throws Exception {
        final StreamExecutionEnvironment env = StreamExecutionEnvironment.getExecutionEnvironment();
        final ParameterTool params = ParameterTool.fromArgs(args);
        env.getConfig().setGlobalJobParameters(params);

        DataStream<String> source = StreamUtil.getDataStream(env, params);

        /*
        @deprecated Please use side output instead.
        //import org.apache.flink.streaming.api.datastream.SplitStream;
        source.split()
        SplitStream<String> split = source.split(new RerouteData());
        select(String s) {
            List<String> outs = new ArrayList<>();
            if (s.startsWith("a")) {
                outs.add("Awords");
            } else {
                outs.add("Others");
            }
            return outs;
        }
        */

        final OutputTag<String> aWordsTag = new OutputTag<String>("A-words") {
        };
        final OutputTag<String> aWordsComplementTag = new OutputTag<String>("A-words-complement") {
        };

        SingleOutputStreamOperator<String> mainDataStream = source
                .process(new ProcessFunction<String, String>() {
                    @Override
                    public void processElement(String value, Context ctx, Collector<String> out) throws Exception {
                        // emit data to regular output
                        out.collect(value);
                        // emit data to side output
                        if (value.startsWith("a")) {
                            ctx.output(aWordsTag, value);
                        } else {
                            ctx.output(aWordsComplementTag, value);
                        }
                    }
                });
        /*
        DataStream<String> awords = split.select("Awords");
        DataStream<String> others = split.select("Othres");
        */
        DataStream<String> awords = mainDataStream.getSideOutput(aWordsTag);
        DataStream<String> others = mainDataStream.getSideOutput(aWordsComplementTag);

        awords.writeAsText("awords", FileSystem.WriteMode.OVERWRITE);
        others.writeAsText("awords-complement", FileSystem.WriteMode.OVERWRITE);
        mainDataStream.writeAsText("out", FileSystem.WriteMode.OVERWRITE);

        env.execute("Split");
    }

    private static Tuple3<Integer, Integer, Integer> addIterCount(Integer value) {
        return Tuple3.of(value, value, 0);
    }

    private static Tuple3<Integer, Integer, Integer> checkMultiple4(Tuple3<Integer, Integer, Integer> value) {
        if (value.f1 % 4 == 0) {
            return value;
        }
        return Tuple3.of(value.f0, value.f1 - 1, value.f2 + 1);
    }
}
