package examples;

import org.apache.flink.api.common.functions.FlatMapFunction;
import org.apache.flink.api.java.utils.ParameterTool;
import org.apache.flink.core.fs.FileSystem;
import org.apache.flink.streaming.api.datastream.DataStream;
import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment;
import org.apache.flink.util.Collector;

public class Ex4FlatMap {

	public static void main(String[] args) throws Exception {
		final StreamExecutionEnvironment env = StreamExecutionEnvironment.getExecutionEnvironment();
		final ParameterTool params = ParameterTool.fromArgs(args);
		DataStream<String> dataStream = StreamUtil.getDataStream(env, params);
		if (dataStream == null) {
			return;
		}
		DataStream<String> outStream = dataStream
				// Extract Areas of Interest
				.map(s -> s.split(":")[1].trim())
				// Split Areas of Interest
				.flatMap(new SplitAreas());
		final String outFile = params.get("output", "output");
		outStream.print();
		outStream.writeAsText(outFile, FileSystem.WriteMode.OVERWRITE);
		env.execute("Example 4: FlatMap");
	}
	public static class SplitAreas implements FlatMapFunction<String, String> {
		@Override
		// "1,nome,valor"
		// --- "1" --- "nome" --- "valor" --->
		public void flatMap(String s, Collector<String> collector) throws Exception {
			for (String area: s.split(",")) {
				collector.collect(area.trim());
				Thread.sleep(1);
			}
		}
	}
}
