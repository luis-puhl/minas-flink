package examples;

import org.apache.flink.api.common.functions.FlatMapFunction;
import org.apache.flink.api.common.functions.MapFunction;
import org.apache.flink.api.java.tuple.Tuple2;
import org.apache.flink.api.java.utils.ParameterTool;
import org.apache.flink.core.fs.FileSystem;
import org.apache.flink.streaming.api.datastream.DataStream;
import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment;
import org.apache.flink.util.Collector;

public class Ex7NumberAgg {

	public static void main(String[] args) throws Exception {
		final StreamExecutionEnvironment env = StreamExecutionEnvironment.getExecutionEnvironment();
		final ParameterTool params = ParameterTool.fromArgs(args);
		DataStream<String> dataStream = StreamUtil.getDataStream(env, params);
		if (dataStream == null) {
			return;
		}
		DataStream<Tuple2<String, Double>> outStream = dataStream
				.map(new RowSplitter())
				.keyBy(0)
				.min(1);
		final String outFile = params.get("output", "output");
		outStream.print();
		outStream.writeAsText(outFile, FileSystem.WriteMode.OVERWRITE);
		env.execute("Example 7: Keyed Streams Min");
	}
	public static class RowSplitter implements MapFunction<String, Tuple2<String, Double>> {
		@Override
		public Tuple2<String, Double> map(String row) throws Exception {
			try {
				String[] fields = row.split(" ");
				if (fields.length == 2) {
					String name = fields[0];
					Double runtime = Double.parseDouble(fields[1]);
					return new Tuple2<>(name, runtime);
				}
			} catch (Exception ex) {
				System.err.println(ex);
			}
			return null;
		}
	}
}
