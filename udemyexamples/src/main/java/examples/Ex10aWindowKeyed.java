package examples;

import org.apache.flink.api.common.functions.MapFunction;
import org.apache.flink.api.java.tuple.Tuple2;
import org.apache.flink.api.java.utils.ParameterTool;
import org.apache.flink.core.fs.FileSystem;
import org.apache.flink.streaming.api.datastream.DataStream;
import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment;
import org.apache.flink.streaming.api.windowing.assigners.TumblingProcessingTimeWindows;
import org.apache.flink.streaming.api.windowing.time.Time;

import java.io.IOException;
import java.io.PrintWriter;
import java.net.ServerSocket;
import java.net.Socket;

public class Ex10aWindowKeyed {
    public static final String host = "127.0.0.1";
    public static final int port = 8080;

    public static void main(String[] args) throws Exception {
        final StreamExecutionEnvironment env = StreamExecutionEnvironment.getExecutionEnvironment();
        final ParameterTool params = ParameterTool.fromArgs(args);
        System.out.println("Reading from tcp " + host + ":" + port);
        DataStream<String> dataStream = env.socketTextStream(host, port);
        if (dataStream == null) {
            return;
        }
        Thread inputThread = new Thread(new LoadInput());
        inputThread.start();
        DataStream<Tuple2<String, Double>> outStream = dataStream
                .map(new MapSource())
                .keyBy(0)
                .window(TumblingProcessingTimeWindows.of(Time.seconds(1)))
                .sum(1);
        final String outFile = params.get("output", "output");
        outStream.print();
		outStream.writeAsText(outFile, FileSystem.WriteMode.OVERWRITE);
		dataStream.writeAsText("random-foods", FileSystem.WriteMode.OVERWRITE);
        env.execute("Example 10a: Keyed Tumbling Window");
        inputThread.join();
    }

    public static class MapSource implements MapFunction<String, Tuple2<String, Double>> {

        @Override
        public Tuple2<String, Double> map(String s) throws Exception {
            String[] composed = s.split(": ");
            String klass = composed[0].trim();
            Double value = Double.parseDouble(composed[1].trim());
            return new Tuple2<String, Double>(klass, value);
        }
    }

    public static class LoadInput implements Runnable {
        @Override
        public void run() {
            ServerSocket serverSocket;
            Socket clientSocket;
            try {
                serverSocket = new ServerSocket(port);
                clientSocket = serverSocket.accept();
                PrintWriter out = new PrintWriter(clientSocket.getOutputStream(), true);
                for (int i = 0; i <= 100; i++) {
                    double random = Math.random() * 49 + 1;
                    String s = (random < 20 ? "pizza" : random > 40 ? "pasta" : "Cannoli") + ": " + random;
                    out.println(s);
                    System.out.println(s);
                    Thread.sleep((int) random * 10);
                }
                clientSocket.close();
                serverSocket.close();
            } catch (IOException | InterruptedException e) {
                e.printStackTrace();
            }
        }
    }
}
