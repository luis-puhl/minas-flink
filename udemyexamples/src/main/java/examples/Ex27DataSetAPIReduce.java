package examples;

import org.apache.flink.api.common.functions.GroupReduceFunction;
import org.apache.flink.api.common.functions.MapFunction;
import org.apache.flink.api.common.functions.ReduceFunction;
import org.apache.flink.api.java.DataSet;
import org.apache.flink.api.java.ExecutionEnvironment;
import org.apache.flink.api.java.operators.DataSource;
import org.apache.flink.api.java.tuple.Tuple2;
import org.apache.flink.api.java.tuple.Tuple3;
import org.apache.flink.api.java.utils.ParameterTool;
import org.apache.flink.core.fs.FileSystem;
import org.apache.flink.streaming.api.datastream.DataStream;
import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment;
import org.apache.flink.util.Collector;

public class Ex27DataSetAPIReduce {
	public static void main(String[] args) throws Exception {
		final ExecutionEnvironment env = ExecutionEnvironment.getExecutionEnvironment();
		final ParameterTool params = ParameterTool.fromArgs(args);
		env.getConfig().setGlobalJobParameters(params);
		DataSource<String> dataSet;
		if (params.has("input")) {
			final String filename = params.get("input");
			System.out.println("Reading from file " + filename);
			dataSet = env.readTextFile(filename);
		} else {
			return;
		}
		DataSet<Tuple2<String, Double>> outStream = dataSet
				.map(new ParseRow())
				.groupBy(0)
				//.reduceGroup(Ex27DataSetAPIReduce::reduce)
				.reduceGroup(new GroupReduceFunction<Tuple3<String, Double, Integer>, Tuple3<String, Double, Integer>>() {
					@Override
					public void reduce(Iterable<Tuple3<String, Double, Integer>> values, Collector<Tuple3<String, Double, Integer>> out) throws Exception {
						String key = null;
						Double sum = 0.0;
						Integer count = 0;
						for (Tuple3<String, Double, Integer> in: values) {
							if (key == null) {
								key = in.f0;
							}
							sum += in.f1;
							count += in.f2;
						}
						out.collect(Tuple3.of(key, sum, count));
					}
				})
				.map(new Average());
		final String outFile = params.get("output", "output");
		outStream.print();
		outStream.writeAsText(outFile, FileSystem.WriteMode.OVERWRITE);
		env.execute("Example 27: Data Set API Reduce");
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

	public static class Average implements  MapFunction<Tuple3<String, Double, Integer>, Tuple2<String, Double>> {
		@Override
		public Tuple2<String, Double> map(Tuple3<String, Double, Integer> in) throws Exception {
			return new Tuple2<>(in.f0, in.f1 / in.f2);
		}
	}
}
