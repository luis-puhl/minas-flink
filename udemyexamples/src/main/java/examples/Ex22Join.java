package examples;

import org.apache.flink.api.common.typeinfo.TypeHint;
import org.apache.flink.api.common.typeinfo.TypeInformation;
import org.apache.flink.api.java.tuple.Tuple3;
import org.apache.flink.api.java.utils.ParameterTool;
import org.apache.flink.core.fs.FileSystem;
import org.apache.flink.streaming.api.datastream.DataStream;
import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment;
import org.apache.flink.streaming.api.windowing.assigners.TumblingProcessingTimeWindows;
import org.apache.flink.streaming.api.windowing.time.Time;

public class Ex22Join {
    public static void main(String[] args) throws Exception {
        final StreamExecutionEnvironment env = StreamExecutionEnvironment.getExecutionEnvironment();
        final ParameterTool params = ParameterTool.fromArgs(args);
        env.getConfig().setGlobalJobParameters(params);

        TypeInformation<Tuple3<String, String, Double>> info = TypeInformation.of(new TypeHint<Tuple3<String, String, Double>>() {
        });
        DataStream<Tuple3<String, String, Double>> source = StreamUtil.getDataStream(env, params)
                .map((String src) -> {
                    Thread.sleep(10);
                    if (src == null || src.length() < 5) {
                        src = "(BRL,35.74316815422196,37.15008714497207)";
                    }
                    String[] srcList = src.trim().replaceAll("[\\(\\)]", "").split(",");
                    Double openValue = Double.parseDouble(srcList[1]);
                    return new Tuple3<>(srcList[0], srcList[1] + " " + srcList[2], openValue);
                }).returns(info);

        // sum numbers
        DataStream<Tuple3<String, String, Double>> stream1 = source
                .map((Tuple3<String, String, Double> value) -> {
                    String input = value.f1;
                    Double sum = 0.0;
                    for (String v : input.split(" ")) {
                        sum += Double.parseDouble(v);
                    }
                    return new Tuple3<String, String, Double>(input, "Sum", sum);
                }).returns(info);
        // multiply
        DataStream<Tuple3<String, String, Double>> stream2 = source
                .map((Tuple3<String, String, Double> value) -> {
                    String input = value.f1;
                    Double product = 1.0;
                    for (String v : input.split(" ")) {
                        product *= Double.parseDouble(v);
                    }
                    return new Tuple3<String, String, Double>(input, "Product", product);
                }).returns(info);

        DataStream<Tuple3<String, Double, Double>> joined = stream1.join(stream2)
                .where((Tuple3<String, String, Double> stream1Value) -> stream1Value.f0)
                .equalTo((Tuple3<String, String, Double> stream2Value) -> stream2Value.f0)
                .window(TumblingProcessingTimeWindows.of(Time.milliseconds(1)))
                .apply(Ex22Join::join);
        joined.print();
        joined.writeAsText("out", FileSystem.WriteMode.OVERWRITE);

        env.execute("Join");
    }

    private static Tuple3<String, Double, Double> join(Tuple3<String, String, Double> first, Tuple3<String, String, Double> second) {
        return Tuple3.of(first.f0, first.f2, second.f2);
    }
}
