package examples;

import org.apache.flink.api.common.functions.FilterFunction;
import org.apache.flink.api.java.utils.ParameterTool;
import org.apache.flink.core.fs.FileSystem;
import org.apache.flink.streaming.api.datastream.DataStream;
import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment;

public class Ex2Filter {

	public static void main(String[] args) throws Exception {
		final StreamExecutionEnvironment env = StreamExecutionEnvironment.getExecutionEnvironment();
		final ParameterTool params = ParameterTool.fromArgs(args);
		DataStream<String> dataStream = StreamUtil.getDataStream(env, params);
		if (dataStream == null) {
			return;
		}
		DataStream<String> outStream = dataStream.filter(new FilterValidLength());
		env.execute("Example 2: Simple Filter");
	}
	public static class FilterValidLength implements FilterFunction<String> {

		@Override
		public boolean filter(String s) throws Exception {
			try {
				Double.parseDouble(s.trim());
				return false;
			} catch (Exception e) {}
			return s.length() > 3;
		}
	}
}
