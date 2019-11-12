package examples;

import org.apache.flink.api.java.utils.ParameterTool;
import org.apache.flink.core.fs.FileSystem;
import org.apache.flink.streaming.api.datastream.DataStream;
import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment;

public class Ex1NopePipe {

	public static void main(String[] args) throws Exception {
		DataStream<String> dataStream = null;
		final StreamExecutionEnvironment env = StreamExecutionEnvironment.getExecutionEnvironment();
		final ParameterTool params = ParameterTool.fromArgs(args);
		env.getConfig().setGlobalJobParameters(params);
		if (params.has("input")) {
		    final String filename = params.get("input");
		    System.out.println("Reading from file " + filename);
		    dataStream = env.readTextFile(filename);
        } else if (params.has("port") && params.has("host")) {
            final String host = params.get("host");
            final int port = params.getInt("port", 8080);
            System.out.println("Reading from tcp " + host + ":" + port);
		    dataStream = env.socketTextStream(host, port);
        } else {
            System.out.println(
                "Usage"+
                "\n\tfor file: \t--input <PATH>;"+
                "\n\tfor tcp socket: \t--host <HOST> --port <PORT>;"+
                "\n\tfor all output: \t--output <PATH>."
            );
            System.exit(1);
            return;
        }
		final String outFile = params.get("output", "output");
		dataStream.print();
		dataStream.writeAsText(outFile, FileSystem.WriteMode.OVERWRITE);
		env.execute("Simple NOP pipe");
	}
}
