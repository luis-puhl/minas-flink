package examples;

import org.apache.flink.api.java.tuple.Tuple3;
import org.apache.flink.api.java.utils.ParameterTool;
import org.apache.flink.core.fs.FileSystem;
import org.apache.flink.streaming.api.datastream.DataStream;
import org.apache.flink.streaming.api.datastream.IterativeStream;
import org.apache.flink.streaming.api.datastream.SingleOutputStreamOperator;
import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment;

public class Ex25Iterate {
    public static void main(String[] args) throws Exception {
        final StreamExecutionEnvironment env = StreamExecutionEnvironment.getExecutionEnvironment();
        final ParameterTool params = ParameterTool.fromArgs(args);
        env.getConfig().setGlobalJobParameters(params);

        DataStream<Integer> source = env.fromElements(32, 17, 30, 27);

        // enable iteration
        IterativeStream<Tuple3<Integer, Integer, Integer>> iterativeStream = source
                .map(Ex25Iterate::addIterCount)
                .iterate();

        SingleOutputStreamOperator<Tuple3<Integer, Integer, Integer>> checkM4Stream = iterativeStream
                .map(Ex25Iterate::checkMultiple4);
        SingleOutputStreamOperator<Tuple3<Integer, Integer, Integer>> feedback = checkM4Stream
                .filter(value -> value.f1 % 4 != 0);
        iterativeStream.closeWith(feedback);
        SingleOutputStreamOperator<Tuple3<Integer, Integer, Integer>> outStream = checkM4Stream
                .filter(value -> value.f1 % 4 == 0);

        outStream.print();
        outStream.writeAsText("out", FileSystem.WriteMode.OVERWRITE);

        env.execute("Iterate");
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
