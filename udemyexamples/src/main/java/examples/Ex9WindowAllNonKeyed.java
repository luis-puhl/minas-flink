package examples;

import org.apache.flink.api.common.functions.MapFunction;
import org.apache.flink.api.common.functions.ReduceFunction;
import org.apache.flink.api.java.tuple.Tuple2;
import org.apache.flink.api.java.tuple.Tuple3;
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

public class Ex9WindowAllNonKeyed {
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
        DataStream<Double> outStream = dataStream
                .map(s -> Double.parseDouble(s))
                .windowAll(TumblingProcessingTimeWindows.of(Time.seconds(1)))
                .sum(0);
        final String outFile = params.get("output", "output");
        outStream.print();
		outStream.writeAsText(outFile, FileSystem.WriteMode.OVERWRITE);
		dataStream.writeAsText("random-delay", FileSystem.WriteMode.OVERWRITE);
        env.execute("Example 9: Non-Keyed Window All");
        inputThread.join();
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
                    out.println(random);
                    System.out.println(random);
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
