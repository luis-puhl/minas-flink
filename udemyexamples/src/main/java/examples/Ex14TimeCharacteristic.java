package examples;

import org.apache.flink.api.common.functions.MapFunction;
import org.apache.flink.api.java.tuple.Tuple2;
import org.apache.flink.api.java.utils.ParameterTool;
import org.apache.flink.core.fs.FileSystem;
import org.apache.flink.streaming.api.TimeCharacteristic;
import org.apache.flink.streaming.api.datastream.DataStream;
import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment;
import org.apache.flink.streaming.api.functions.AssignerWithPeriodicWatermarks;
import org.apache.flink.streaming.api.functions.AssignerWithPunctuatedWatermarks;
import org.apache.flink.streaming.api.watermark.Watermark;
import org.apache.flink.streaming.api.windowing.assigners.ProcessingTimeSessionWindows;
import org.apache.flink.streaming.api.windowing.assigners.TumblingEventTimeWindows;
import org.apache.flink.streaming.api.windowing.time.Time;

import javax.annotation.Nullable;
import java.io.IOException;
import java.io.PrintWriter;
import java.io.Serializable;
import java.net.ServerSocket;
import java.net.Socket;

public class Ex14TimeCharacteristic {
    public static final String host = "127.0.0.1";
    public static final int port = 8080;

    public static void main(String[] args) throws Exception {
        final StreamExecutionEnvironment env = StreamExecutionEnvironment.getExecutionEnvironment();
        final ParameterTool params = ParameterTool.fromArgs(args);
        System.out.println("Reading from tcp " + host + ":" + port);
        env.setStreamTimeCharacteristic(TimeCharacteristic.EventTime);
        DataStream<String> dataStream = env.socketTextStream(host, port);
        dataStream.writeAsText("random-messages", FileSystem.WriteMode.OVERWRITE);
        dataStream = dataStream.assignTimestampsAndWatermarks(new CustomTimeExtractor());
        if (dataStream == null) {
            return;
        }
        Thread inputThread = new Thread(new LoadInput());
        inputThread.start();
        DataStream<SourceEventPOJO> outStream = dataStream
                .map(s -> SourceEventPOJO.fromString(s))
                .keyBy("key")
                .window(TumblingEventTimeWindows.of(Time.seconds(1)))
                .sum("value");
        final String outFile = params.get("output", "output");
        outStream.print();
		outStream.writeAsText(outFile, FileSystem.WriteMode.OVERWRITE);
        env.execute("Example 13: Time Characteristic");
        inputThread.join();
    }

    public static class SourceEventPOJO {
        public String key;
        public Integer value;
        public Long time;
        public static SourceEventPOJO fromString(String s) {
            String[] split = s.split(",");
            return new SourceEventPOJO(
                split[0].trim(),
                Integer.parseInt(split[1].trim()),
                Long.parseLong(split[2].trim())
            );
        }
        public SourceEventPOJO() {}
        public SourceEventPOJO(String key, Integer value, Long time) {
            this.key = key;
            this.value = value;
            this.time = time;
        }

        @Override
        public String toString() {
            return key + "," + value + "," + time;
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
                    String key = random < 20 ? "pizza" : random > 40 ? "pasta" : "Cannoli";
                    SourceEventPOJO ev = new SourceEventPOJO(key, Integer.valueOf((int)random), System.currentTimeMillis());
                    out.println(ev);
                    System.out.println(ev);
                    Thread.sleep((int) random * 10);
                }
                clientSocket.close();
                serverSocket.close();
            } catch (IOException | InterruptedException e) {
                e.printStackTrace();
            }
        }
    }

    public static class CustomTimeExtractor implements Serializable, AssignerWithPunctuatedWatermarks<String> {
        @Nullable
        @Override
        public Watermark checkAndGetNextWatermark(String s, long l) {
            return new Watermark(this.extractTimestamp(s, l));
        }

        @Override
        public long extractTimestamp(String s, long l) {
            return Long.parseLong(s.split(",")[2].trim());
        }
    }
}
