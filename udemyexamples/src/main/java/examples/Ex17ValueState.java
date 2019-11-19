package examples;

import org.apache.flink.api.common.functions.FlatMapFunction;
import org.apache.flink.api.common.functions.RichFlatMapFunction;
import org.apache.flink.api.common.state.ValueState;
import org.apache.flink.api.common.state.ValueStateDescriptor;
import org.apache.flink.api.common.typeinfo.TypeHint;
import org.apache.flink.api.common.typeinfo.TypeInformation;
import org.apache.flink.api.java.tuple.Tuple2;
import org.apache.flink.api.java.utils.ParameterTool;
import org.apache.flink.configuration.Configuration;
import org.apache.flink.core.fs.FileSystem;
import org.apache.flink.streaming.api.datastream.DataStream;
import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment;
import org.apache.flink.util.Collector;

import java.util.HashMap;
import java.util.Map;

import org.slf4j.LoggerFactory;
import org.slf4j.Logger;

public class Ex17ValueState {
    private static final Logger LOG = LoggerFactory.getLogger(Ex17ValueState.class);

    public static void main(String[] args) throws Exception {
        final StreamExecutionEnvironment env = StreamExecutionEnvironment.getExecutionEnvironment();
        final ParameterTool params = ParameterTool.fromArgs(args);
        env.getConfig().setGlobalJobParameters(params);

        DataStream<String> streamSource = StreamUtil.getDataStream(env, params);
        DataStream<String> wordCountSource = streamSource
                .flatMap(new WordToTuple())
                .keyBy(0)
                .flatMap(new AllWordCounts());
        wordCountSource.print();
        wordCountSource.writeAsText("out", FileSystem.WriteMode.OVERWRITE);

        env.execute("Value State");
    }
    public static class WordToTuple implements FlatMapFunction<String, Tuple2<Integer, String>> {
        @Override
        public void flatMap(String value, Collector<Tuple2<Integer, String>> out) throws Exception {
            String[] words = value.trim().toLowerCase().split("\\W");
            for (String word: words) {
                out.collect(Tuple2.of(1, word));
            }
        }
    }
    public static class AllWordCounts extends RichFlatMapFunction<Tuple2<Integer, String>, String> {
        private transient ValueState<Map<String, Integer>> allWordCounts;

        @Override
        public void flatMap(Tuple2<Integer, String> input, Collector<String> out) throws Exception {
            Map<String, Integer> currentWordCounts = allWordCounts.value();
            if (currentWordCounts == null) {
                LOG.info("currentWordCounts == null");
                // default value
                currentWordCounts = new HashMap<>();
            }
            Integer curr = currentWordCounts.getOrDefault(input.f1, 0) + 1;
            currentWordCounts.put(input.f1, curr);
            allWordCounts.update(currentWordCounts);
            if (input.f1.equals("print")) {
                out.collect(currentWordCounts.toString());
                LOG.info("print -> " + currentWordCounts.toString());
                allWordCounts.clear();
            }
            LOG.info(input.toString() + " -> " + currentWordCounts.toString());
        }

        @Override
        public void open(Configuration parameters) throws Exception {
            ValueStateDescriptor<Map<String, Integer>> descriptor = new ValueStateDescriptor<>(
                    "allWordCounts",
                    TypeInformation.of(new TypeHint<Map<String, Integer>>() {})
            );
            allWordCounts = getRuntimeContext().getState(descriptor);
        }
    }
}
