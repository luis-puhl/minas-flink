package examples;

import org.apache.flink.api.java.tuple.Tuple2;
import org.apache.flink.api.java.tuple.Tuple3;
import org.apache.flink.api.java.tuple.Tuple4;
import org.apache.flink.api.java.utils.ParameterTool;
import org.apache.flink.core.fs.FileSystem;
import org.apache.flink.streaming.api.datastream.DataStream;
import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment;
import org.apache.flink.streaming.api.windowing.assigners.TumblingProcessingTimeWindows;
import org.apache.flink.streaming.api.windowing.time.Time;
import org.apache.flink.util.Collector;

import java.util.ArrayList;
import java.util.List;

public class Ex23CoGroup {
    final static String PRODUCTION_KEY = "production";
    final static String HOURS_KEY = "hours";
    private static List<String> generateCollection() {
        List<String> workers = new ArrayList<>(10);
        workers.add("Luella");
        workers.add("Ann");
        workers.add("May");
        workers.add("Martha");
        workers.add("Frank");
        workers.add("Jon");
        workers.add("Allie");
        workers.add("Roxie");
        workers.add("Agnes");
        workers.add("Alexander");
        workers.add("Alberta");
        List<String> collection = new ArrayList<>(100);
        int workerCount = workers.size();
        for (int i = 0; i < 100; i++) {
            String cmd = Math.random() > 0.5 ? PRODUCTION_KEY : HOURS_KEY;
            String worker = workers.get((int) ((Math.random() * workerCount) % workerCount));
            Double value = Math.random() * 50;
            Double delay = Math.random() * 100;
            Tuple4<String, String, Double, Integer> entry = Tuple4.of(cmd, worker, value, delay.intValue());
            collection.add(entry.toString());
        }
        return collection;
    }

    private static class WorkerHours {
        public String name;
        public Double hours;
    }
    private static class WorkerProduction {
        public String name;
        public Double production;
    }
    private static Tuple4<String, String, Double, Integer> sourceMap(String src) throws InterruptedException {
        String[] split = src.replaceAll("[()]", "").split(",");
        String cmd = split[0];
        String worker = split[1];
        Double value = Double.parseDouble(split[2]);
        Integer delay = Integer.parseInt(split[3]);
        Thread.sleep(delay);
        return Tuple4.of(cmd, worker, value, delay);
    }
    private static Tuple2<String, Double> mainMap(Tuple4<String, String, Double, Integer> src) {
        // worker, value
        return Tuple2.of(src.f1, src.f2);
    }
    private static String streamKeySelector(Tuple2<String, Double> value) {
        return value.f0;
    }
    private static void coGroup(Iterable<Tuple2<String, Double>> first, Iterable<Tuple2<String, Double>> second, Collector<Tuple2<String, Double>> out) {
        String key = null;
        Double hours = 0.0;
        Double production = 0.0;
        for (Tuple2<String, Double> hoursRow: first) {
            key = key == null ? hoursRow.f0 : key;
            hours += hoursRow.f1;
        }
        for (Tuple2<String, Double> productionRow: second) {
            key = key == null ? productionRow.f0 : key;
            production += productionRow.f1;
        }
        Double average = production / hours;
        if (average.isInfinite()) {
            average = 0.0;
        }
        out.collect(Tuple2.of(key, average));
    }

    public static void main(String[] args) throws Exception {
        final StreamExecutionEnvironment env = StreamExecutionEnvironment.getExecutionEnvironment();
        final ParameterTool params = ParameterTool.fromArgs(args);
        env.getConfig().setGlobalJobParameters(params);

        DataStream<Tuple4<String, String, Double, Integer>> source = env
                .fromCollection(generateCollection())
                .map(Ex23CoGroup::sourceMap);
        source.writeAsText("worker-production", FileSystem.WriteMode.OVERWRITE);
        DataStream<Tuple2<String, Double>> production = source
                .filter(value -> value.f0.equals(PRODUCTION_KEY))
                .map(Ex23CoGroup::mainMap);
        DataStream<Tuple2<String, Double>> hours = source
                .filter(value -> value.f0.equals(HOURS_KEY))
                .map(Ex23CoGroup::mainMap);

        // sum numbers
        DataStream<Tuple2<String, Double>> coGroup = production.coGroup(hours)
                .where(Ex23CoGroup::streamKeySelector).equalTo(Ex23CoGroup::streamKeySelector)
                .window(TumblingProcessingTimeWindows.of(Time.seconds(1)))
                .apply(Ex23CoGroup::coGroup);
        coGroup.print();
        coGroup.writeAsText("out", FileSystem.WriteMode.OVERWRITE);

        env.execute("Window Co-Group");
    }
}
