package examples;

import org.apache.flink.api.common.functions.FlatMapFunction;
import org.apache.flink.api.common.functions.RichFlatMapFunction;
import org.apache.flink.api.common.state.ListState;
import org.apache.flink.api.common.state.ListStateDescriptor;
import org.apache.flink.api.java.tuple.Tuple2;
import org.apache.flink.api.java.utils.ParameterTool;
import org.apache.flink.configuration.Configuration;
import org.apache.flink.core.fs.FileSystem;
import org.apache.flink.streaming.api.datastream.DataStream;
import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment;
import org.apache.flink.util.Collector;

public class Ex18ListState {
    public static void main(String[] args) throws Exception {
        final StreamExecutionEnvironment env = StreamExecutionEnvironment.getExecutionEnvironment();
        final ParameterTool params = ParameterTool.fromArgs(args);
        env.getConfig().setGlobalJobParameters(params);

        DataStream<String> wordCountSource = StreamUtil.getDataStream(env, params)
                .flatMap(new WordToTuple())
                .keyBy(0)
                .flatMap(new CollectDistinctWords());
        wordCountSource.writeAsText("out", FileSystem.WriteMode.OVERWRITE);

        env.execute("List State");
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

    public static class CollectDistinctWords extends RichFlatMapFunction<Tuple2<Integer, String>, String> {
        private transient ListState<String> distinctWordsList;

        @Override
        public void flatMap(Tuple2<Integer, String> input, Collector<String> out) throws Exception {
            Iterable<String> currentWordCounts = distinctWordsList.get();
            if (input.f1.equals("print")) {
                out.collect(currentWordCounts.toString());
                distinctWordsList.clear();
                return;
            }
            for (String word : currentWordCounts) {
                if (input.f1.equals(word)) {
                    return;
                }
            }
            distinctWordsList.add(input.f1);
        }

        @Override
        public void open(Configuration parameters) throws Exception {
            ListStateDescriptor<String> descriptor = new ListStateDescriptor<>("distinctWordsList", String.class);
            distinctWordsList = getRuntimeContext().getListState(descriptor);
        }
    }
}
