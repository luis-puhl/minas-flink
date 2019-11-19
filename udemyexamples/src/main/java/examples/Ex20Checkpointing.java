package examples;

import org.apache.flink.api.common.functions.RichFlatMapFunction;
import org.apache.flink.api.common.restartstrategy.RestartStrategies;
import org.apache.flink.api.common.state.ReducingState;
import org.apache.flink.api.common.state.ReducingStateDescriptor;
import org.apache.flink.api.java.tuple.Tuple2;
import org.apache.flink.api.java.utils.ParameterTool;
import org.apache.flink.configuration.Configuration;
import org.apache.flink.runtime.state.filesystem.FsStateBackend;
import org.apache.flink.streaming.api.CheckpointingMode;
import org.apache.flink.streaming.api.datastream.DataStream;
import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment;
import org.apache.flink.streaming.api.functions.sink.RichSinkFunction;
import org.apache.flink.streaming.api.functions.sink.SinkFunction;
import org.apache.flink.util.Collector;

import java.io.BufferedWriter;
import java.io.FileWriter;
import java.io.IOException;
import java.io.PrintWriter;

public class Ex20Checkpointing {
    public static void main(String[] args) throws Exception {
        final StreamExecutionEnvironment env = StreamExecutionEnvironment.getExecutionEnvironment();
        final ParameterTool params = ParameterTool.fromArgs(args);
        env.getConfig().setGlobalJobParameters(params);

        // start a checkpoint every 1000ms
        env.enableCheckpointing(50);
        // env.getCheckpointConfig().setCheckpointingMode(CheckpointingMode.AT_LEAST_ONCE);
        env.getCheckpointConfig().setCheckpointingMode(CheckpointingMode.EXACTLY_ONCE);
        env.getCheckpointConfig().setMinPauseBetweenCheckpoints(50);
        // allow only one to be processed at the same time
        env.getCheckpointConfig().setMaxConcurrentCheckpoints(2);
        //noinspection deprecation
        env.setStateBackend(new FsStateBackend("file:///tmp/flink/state", false));
        env.setRestartStrategy(RestartStrategies.fixedDelayRestart(2, 100));

        DataStream<String> wordCountSource = StreamUtil.getDataStream(env, params)
                .map(Info::fromString)
                .returns(Info.class)
                .keyBy("base")
                .flatMap(new SumInfo())
                .flatMap((Info info, Collector<String> out) -> {
                    info.isError = Math.random() > .75;
                    out.collect(info.toString());
                    if (info.isError) {
                        throw new Exception("Random error");
                    }
                })
                .returns(String.class);
        final AppendFileSink sink = new AppendFileSink("out");
        wordCountSource.addSink(sink);

        env.execute("Checkpointing and failure");
    }

    public static class AppendFileSink extends RichSinkFunction<String> {
        String path = "out";
        PrintWriter file;
        FileWriter fw;
        BufferedWriter bw;
        AppendFileSink(String path) {
            this.path = path;
        }
        @Override
        public void open(Configuration parameters) throws Exception {
            super.open(parameters);
            try {
                fw = new FileWriter(path, true);
                bw = new BufferedWriter(fw);
                file = new PrintWriter(bw);
            } catch (IOException e) {
                //exception handling left as an exercise for the reader
            }
        }

        @Override
        public void close() throws Exception {
            super.close();
            file.flush();
            file.close();
            bw.flush();
            bw.close();
            fw.flush();
            fw.close();
        }

        @Override
        public void invoke(String value, SinkFunction.Context context) throws Exception {
            file.println(value);
        }
    }

    public static class Info {
        public static Info fromString(String src) throws InterruptedException {
            Integer value = ((Double) Double.parseDouble(src)).intValue();
            Thread.sleep(value);
            return new Info(0, false, value, value);
        }
        public Integer base;
        public Boolean isError;
        public Integer value;
        public Integer sum;

        public Info() {}

        public Info(Integer base, Boolean isError, Integer value, Integer sum) {
            this.base = base;
            this.isError = isError;
            this.value = value;
            this.sum = sum;
        }

        @Override
        public String toString() {
            return this.sum.toString() + "\t" + this.value.toString() + "\t" + (this.isError ? "fail" : "pass");
        }
    }

    public static class SumInfo extends RichFlatMapFunction<Info, Info> {
        private transient ReducingState<Info> infoReducingState;

        @Override
        public void open(Configuration parameters) throws Exception {
            ReducingStateDescriptor<Info> descriptor = new ReducingStateDescriptor<>(
                    "TotalWordCount",
                    (Info acc, Info in) -> new Info(in.base, in.isError, in.value, acc.sum + in.sum),
                    Info.class
            );
            infoReducingState = getRuntimeContext().getReducingState(descriptor);
        }

        @Override
        public void flatMap(Info value, Collector<Info> out) throws Exception {
            infoReducingState.add(value);
            out.collect(infoReducingState.get());
        }
    }

}
