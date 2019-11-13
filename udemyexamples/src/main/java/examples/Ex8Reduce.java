package examples;

import org.apache.flink.api.common.functions.MapFunction;
import org.apache.flink.api.common.functions.ReduceFunction;
import org.apache.flink.api.java.tuple.Tuple2;
import org.apache.flink.api.java.tuple.Tuple3;
import org.apache.flink.api.java.utils.ParameterTool;
import org.apache.flink.core.fs.FileSystem;
import org.apache.flink.streaming.api.datastream.DataStream;
import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment;

public class Ex8Reduce {

	public static void main(String[] args) throws Exception {
		final StreamExecutionEnvironment env = StreamExecutionEnvironment.getExecutionEnvironment();
		final ParameterTool params = ParameterTool.fromArgs(args);
		DataStream<String> dataStream = StreamUtil.getDataStream(env, params);
		if (dataStream == null) {
			return;
		}
		DataStream<Tuple2<String, Double>> outStream = dataStream
				.map(new ParseRow())
				.keyBy(0)
				.reduce(new SumAndCount())
				.map(new Average());
		final String outFile = params.get("output", "output");
		outStream.print();
		outStream.writeAsText(outFile, FileSystem.WriteMode.OVERWRITE);
		env.execute("Example 7: Keyed Streams Min");
	}
	public static class ParseRow implements MapFunction<String, Tuple3<String, Double, Integer>> {
		@Override
		public Tuple3<String, Double, Integer> map(String row) throws Exception {
			try {
				String[] fields = row.split(",");
				if (fields.length == 3) {
					String name = fields[0].trim();
					String area = fields[1].trim();
					Double length = Double.parseDouble(fields[2]);
					return new Tuple3<>(area, length, 1);
				}
			} catch (Exception ex) {
				System.err.println(ex);
			}
			return null;
		}
	}

	public static class SumAndCount implements ReduceFunction<Tuple3<String, Double, Integer>> {
		@Override
		public Tuple3<String, Double, Integer> reduce(Tuple3<String, Double, Integer> acc, Tuple3<String, Double, Integer> input) throws Exception {
			return new Tuple3<>(input.f0, acc.f1 + input.f1, acc.f2 + 1);
		}
	}

	public static class Average implements  MapFunction<Tuple3<String, Double, Integer>, Tuple2<String, Double>> {
		@Override
		public Tuple2<String, Double> map(Tuple3<String, Double, Integer> in) throws Exception {
			return new Tuple2<>(in.f0, in.f1 / in.f2);
		}
	}
}
