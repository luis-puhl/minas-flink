package examples;

import org.apache.flink.api.common.functions.FlatMapFunction;
import org.apache.flink.api.common.functions.RichFlatMapFunction;
import org.apache.flink.api.common.state.ReducingState;
import org.apache.flink.api.common.state.ReducingStateDescriptor;
import org.apache.flink.api.java.tuple.Tuple2;
import org.apache.flink.api.java.utils.ParameterTool;
import org.apache.flink.configuration.Configuration;
import org.apache.flink.core.fs.FileSystem;
import org.apache.flink.streaming.api.datastream.DataStream;
import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment;
import org.apache.flink.util.Collector;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class Ex19ReducingState {
    private static final Logger LOG = LoggerFactory.getLogger(Ex19ReducingState.class);

    public static void main(String[] args) throws Exception {
        final StreamExecutionEnvironment env = StreamExecutionEnvironment.getExecutionEnvironment();
        final ParameterTool params = ParameterTool.fromArgs(args);
        env.getConfig().setGlobalJobParameters(params);

        DataStream<String> streamSource = StreamUtil.getDataStream(env, params);
        DataStream<String> wordCountSource = streamSource
                .flatMap(new WordToTuple())
                .keyBy(0)
                .flatMap(new TotalWordCount());
        wordCountSource.print();
        wordCountSource.writeAsText("out", FileSystem.WriteMode.OVERWRITE);

        env.execute("Reducing State");
    }

    public static class WordToTuple implements FlatMapFunction<String, Tuple2<Integer, String>> {
        @Override
        public void flatMap(String value, Collector<Tuple2<Integer, String>> out) throws Exception {
            String[] words = value.trim().toLowerCase().split("\\W");
            for (String word : words) {
                out.collect(Tuple2.of(1, word));
            }
        }
    }

    public static class TotalWordCount extends RichFlatMapFunction<Tuple2<Integer, String>, String> {
        private transient ReducingState<Integer> allWordCounts;

        @Override
        public void flatMap(Tuple2<Integer, String> input, Collector<String> out) throws Exception {
            if (input.f1.equals("print")) {
                out.collect(allWordCounts.get().toString());
                allWordCounts.clear();
                return;
            }
            allWordCounts.add(1);
        }

        @Override
        public void open(Configuration parameters) throws Exception {
            ReducingStateDescriptor<Integer> descriptor = new ReducingStateDescriptor<>(
                    "TotalWordCount",
                    (acc, in) -> acc + in,
                    Integer.class
            );
            allWordCounts = getRuntimeContext().getReducingState(descriptor);
        }
    }
}
