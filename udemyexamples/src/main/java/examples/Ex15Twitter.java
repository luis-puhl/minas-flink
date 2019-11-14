package examples;

import org.apache.flink.api.common.functions.FlatMapFunction;
import org.apache.flink.api.common.functions.MapFunction;
import org.apache.flink.api.java.tuple.Tuple2;
import org.apache.flink.api.java.utils.ParameterTool;
import org.apache.flink.core.fs.FileSystem;
import org.apache.flink.shaded.jackson2.com.fasterxml.jackson.databind.JsonNode;
import org.apache.flink.shaded.jackson2.com.fasterxml.jackson.databind.ObjectMapper;
import org.apache.flink.streaming.api.TimeCharacteristic;
import org.apache.flink.streaming.api.datastream.DataStream;
import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment;
import org.apache.flink.streaming.api.functions.AssignerWithPunctuatedWatermarks;
import org.apache.flink.streaming.api.watermark.Watermark;
import org.apache.flink.streaming.api.windowing.assigners.TumblingEventTimeWindows;
import org.apache.flink.streaming.api.windowing.assigners.TumblingProcessingTimeWindows;
import org.apache.flink.streaming.api.windowing.time.Time;
import org.apache.flink.streaming.connectors.twitter.TwitterSource;
import org.apache.flink.util.Collector;

import javax.annotation.Nullable;
import java.io.IOException;
import java.io.PrintWriter;
import java.io.Serializable;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.StringTokenizer;

public class Ex15Twitter {
    public static void main(String[] args) throws Exception {
        final StreamExecutionEnvironment env = StreamExecutionEnvironment.getExecutionEnvironment();
        final ParameterTool params = ParameterTool.fromArgs(args);
        env.getConfig().setGlobalJobParameters(params);

        if (!(params.has(TwitterSource.CONSUMER_KEY) &&
                params.has(TwitterSource.CONSUMER_SECRET) &&
                params.has(TwitterSource.TOKEN) &&
                params.has(TwitterSource.TOKEN_SECRET))
        ) {
            System.out.println("Executing TwitterStream example with default props.");
            System.out.println(
                    "Use --twitter-source.consumerKey <key> --twitter-source.consumerSecret <secret> " +
                    "--twitter-source.token <token> --twitter-source.tokenSecret <tokenSecret> specify the authentication info."
            );
            return;
        }
        DataStream<String> streamSource = env.addSource(new TwitterSource(params.getProperties()));
        // DataStream<Tuple2<String, Integer>> tweets = streamSource
        //         .flatMap(new SelectEnglishAndTokenizeFlatMap())
        //         .keyBy(0).sum(1);
        DataStream<Tuple2<String, Integer>> tweets = streamSource
                .map(new MapString2Json())
                .map(new MapTweetToLocation())
                .keyBy(0)
                .window(TumblingProcessingTimeWindows.of(Time.seconds(1)))
                .sum(1);

        if (params.has("output")) {
            tweets.writeAsText(params.get("output"), FileSystem.WriteMode.OVERWRITE);
        } else {
            System.out.println("Printing result to stdout. Use --output to specify output path.");
            tweets.print();
        }

        env.execute("Twitter Streaming Example");
    }

    public static class MapString2Json implements MapFunction<String, JsonNode> {
        private static final long serialVersionUID = 1L;
        private transient ObjectMapper jsonParser;

        @Override
        public JsonNode map(String value) throws Exception {
            if (jsonParser == null) {
                jsonParser = new ObjectMapper();
            }
            JsonNode jsonNode = jsonParser.readValue(value, JsonNode.class);
            return jsonNode;
        }
    }
    public static class MapTweetToLocation implements MapFunction<JsonNode, Tuple2<String, Integer>> {
        @Override
        public Tuple2<String, Integer> map(JsonNode node) throws Exception {
            String location = "unknown";
            if (node.has("user") && node.get("user").has("location")) {
                location = node.get("user").get("location").asText();
            }
            return new Tuple2<String, Integer>(location, 1);
        }
    }

    public static class SelectEnglishAndTokenizeFlatMap implements FlatMapFunction<String, Tuple2<String, Integer>> {
        private static final long serialVersionUID = 1L;
        private transient ObjectMapper jsonParser;
        @Override
        public void flatMap(String value, Collector<Tuple2<String, Integer>> collector) throws Exception {
            if (jsonParser == null) {
                jsonParser = new ObjectMapper();
            }
            JsonNode jsonNode = jsonParser.readValue(value, JsonNode.class);
            boolean isEnglish = jsonNode.has("user") && jsonNode.get("user").has("lang") && jsonNode.get("user").get("lang").asText().equals("en");
            boolean hasText = jsonNode.has("text");
            if (isEnglish && hasText) {
                // message of tweet
                StringTokenizer tokenizer = new StringTokenizer(jsonNode.get("text").asText());

                // split the message
                while (tokenizer.hasMoreTokens()) {
                    String result = tokenizer.nextToken().replaceAll("\\s*", "").toLowerCase();

                    if (!result.equals("")) {
                        collector.collect(new Tuple2<>(result, 1));
                    }
                }
            }
        }
    }
}
