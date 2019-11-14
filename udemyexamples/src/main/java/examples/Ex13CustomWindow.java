package examples;

import org.apache.flink.api.common.functions.MapFunction;
import org.apache.flink.api.java.tuple.Tuple;
import org.apache.flink.api.java.tuple.Tuple2;
import org.apache.flink.api.java.utils.ParameterTool;
import org.apache.flink.core.fs.FileSystem;
import org.apache.flink.streaming.api.datastream.DataStream;
import org.apache.flink.streaming.api.datastream.SingleOutputStreamOperator;
import org.apache.flink.streaming.api.datastream.WindowedStream;
import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment;
import org.apache.flink.streaming.api.functions.windowing.WindowFunction;
import org.apache.flink.streaming.api.windowing.windows.GlobalWindow;
import org.apache.flink.util.Collector;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

public class Ex13CustomWindow {

    public static void main(String[] args) throws Exception {
        final StreamExecutionEnvironment env = StreamExecutionEnvironment.getExecutionEnvironment();
        final ParameterTool params = ParameterTool.fromArgs(args);
        DataStream<String> dataStream = StreamUtil.getDataStream(env, params);
        if (dataStream == null) {
            return;
        }
        WindowedStream<CustomCourseCountPOJO, Tuple, GlobalWindow> windowedStream = dataStream
                .map(s -> {
                    String[] composed = s.split(",");
                    String course = composed[0].trim();
                    String country = composed[1].trim();
                    Double value = Double.parseDouble(composed[2].trim());
                    return new CustomCourseCountPOJO(course, country, value);
                })
                .keyBy("staticKey")
                .countWindow(5);
        SingleOutputStreamOperator<Map<String, Integer>> outStream = windowedStream.apply(new CollectUS());
        final String outFile = params.get("output", "output");
        outStream.print();
		outStream.writeAsText(outFile, FileSystem.WriteMode.OVERWRITE);
        env.execute("Example 10a: Keyed Tumbling Window");
    }

    public static class MapSource implements MapFunction<String, Tuple2<String, Double>> {

        @Override
        public Tuple2<String, Double> map(String s) throws Exception {
            String[] composed = s.split(": ");
            String klass = composed[0].trim();
            Double value = Double.parseDouble(composed[1].trim());
            return new Tuple2<String, Double>(klass, 1.0);
        }
    }

    public static class CollectUS implements WindowFunction<CustomCourseCountPOJO, Map<String, Integer>, Tuple, GlobalWindow> {
        @Override
        public void apply(Tuple tuple, GlobalWindow globalWindow, Iterable<CustomCourseCountPOJO> iterable, Collector<Map<String, Integer>> collector) throws Exception {
            Map<String, Integer> out = new HashMap<>();
            for (CustomCourseCountPOJO singUp: iterable) {
                if (singUp.country.matches("[Uu][Ss]")) {
                    String course = singUp.course.toLowerCase();
                    Integer current = out.get(course) == null ? 0 : out.get(course);
                    out.put(course, current + 1);
                }
            }
            collector.collect(out);
        }
    }

    public static class CustomCourseCountPOJO {
        public Integer staticKey = 0;
        public String course;
        public String country;
        public Double timestamp;

        public CustomCourseCountPOJO() {}
        public CustomCourseCountPOJO(String course, String country, Double timestamp) {
            this.course = course;
            this.country = country;
            this.timestamp = timestamp;
        }

        @Override
        public String toString() {
            ArrayList<String> fields = new ArrayList<>(3);
            fields.add(course);
            fields.add(country);
            fields.add(timestamp.toString());
            return String.join(",", fields);
        }
    }
}
