package examples;

import org.apache.flink.api.common.functions.FilterFunction;
import org.apache.flink.api.common.functions.MapFunction;
import org.apache.flink.api.java.utils.ParameterTool;
import org.apache.flink.core.fs.FileSystem;
import org.apache.flink.streaming.api.datastream.DataStream;
import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment;

public class Ex3FilterMap {

	public static void main(String[] args) throws Exception {
		final StreamExecutionEnvironment env = StreamExecutionEnvironment.getExecutionEnvironment();
		final ParameterTool params = ParameterTool.fromArgs(args);
		DataStream<String> dataStream = StreamUtil.getDataStream(env, params);
		if (dataStream == null) {
			return;
		}
		DataStream<String> outStream = dataStream
				.filter(new FilterValidLength())
				.map(new CleanStringMap());
		final String outFile = params.get("output", "output");
		outStream.print();
		outStream.writeAsText(outFile, FileSystem.WriteMode.OVERWRITE);
		env.execute("Example 3: Filter and Map");
	}
	public static class FilterValidLength implements FilterFunction<String> {

		@Override
		public boolean filter(String s) throws Exception {
			try {
				Double.parseDouble(s.trim());
				return false;
			} catch (Exception e) {}
			return s.length() > 4;
		}
	}
	public static class CleanStringMap implements MapFunction<String, String> {

		@Override
		public String map(String s) throws Exception {
			return s.trim().toLowerCase();
		}
	}
}
