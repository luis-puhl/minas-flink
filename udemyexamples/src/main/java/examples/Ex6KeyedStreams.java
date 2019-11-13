package examples;

import org.apache.flink.api.common.functions.FlatMapFunction;
import org.apache.flink.api.java.tuple.Tuple2;
import org.apache.flink.api.java.utils.ParameterTool;
import org.apache.flink.core.fs.FileSystem;
import org.apache.flink.streaming.api.datastream.DataStream;
import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment;
import org.apache.flink.util.Collector;

public class Ex6KeyedStreams {

	public static void main(String[] args) throws Exception {
		final StreamExecutionEnvironment env = StreamExecutionEnvironment.getExecutionEnvironment();
		final ParameterTool params = ParameterTool.fromArgs(args);
		DataStream<String> dataStream = StreamUtil.getDataStream(env, params);
		if (dataStream == null) {
			return;
		}
		DataStream<Tuple2<String, Integer>> outStream = dataStream
				// Extract Movie Name
				.map(s -> s.split(":")[1].trim())
				// Split Cast Actors
				.flatMap(new SplitCastActors())
				.keyBy(0)
				.sum(1);
		final String outFile = params.get("output", "output");
		outStream.print();
		outStream.writeAsText(outFile, FileSystem.WriteMode.OVERWRITE);
		env.execute("Example 6: Keyed Streams Sum");
	}
	public static class SplitCastActors implements FlatMapFunction<String, Tuple2<String, Integer>> {
		@Override
		public void flatMap(String s, Collector<Tuple2<String, Integer>> collector) throws Exception {
			for (String actor: s.split(",")) {
				collector.collect(new Tuple2<String, Integer>(actor.trim(), 1));
			}
		}
	}
}
