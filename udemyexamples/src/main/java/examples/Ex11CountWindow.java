package examples;

import org.apache.flink.api.common.functions.MapFunction;
import org.apache.flink.api.java.utils.ParameterTool;
import org.apache.flink.core.fs.FileSystem;
import org.apache.flink.streaming.api.datastream.DataStream;
import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment;

public class Ex11CountWindow {
    public static void main(String[] args) throws Exception {
        final StreamExecutionEnvironment env = StreamExecutionEnvironment.getExecutionEnvironment();
        final ParameterTool params = ParameterTool.fromArgs(args);
        DataStream<String> dataStream = StreamUtil.getDataStream(env, params);
        if (dataStream == null) {
            return;
        }
        DataStream<CourseCountPOJO> outStream = dataStream
                .map(new MapSource())
                .map(src -> new CourseCountPOJO("", src.area, src.count))
                .keyBy("area")
                .countWindow(2)
                .sum("count");
        final String outFile = params.get("output", "output");
        outStream.print();
		outStream.writeAsText(outFile, FileSystem.WriteMode.OVERWRITE);
        env.execute("Example 11: Count Window");
    }

    public static class MapSource implements MapFunction<String, CourseCountPOJO> {

        @Override
        public CourseCountPOJO map(String s) throws Exception {
            String[] composed = s.split(",");
            String course = composed[0].trim();
            String area = composed[1].trim();
            Integer value = Integer.parseInt(composed[2].trim());
            return new CourseCountPOJO(course, area, value);
        }
    }
}
