package examples;

import org.apache.flink.api.common.functions.MapFunction;
import org.apache.flink.api.java.DataSet;
import org.apache.flink.api.java.ExecutionEnvironment;
import org.apache.flink.api.java.operators.DataSource;
import org.apache.flink.api.java.tuple.Tuple2;
import org.apache.flink.api.java.tuple.Tuple3;
import org.apache.flink.api.java.utils.ParameterTool;
import org.apache.flink.core.fs.FileSystem;
import org.apache.flink.graph.Graph;

import java.util.ArrayList;
import java.util.LinkedList;
import java.util.List;

public class Ex28GraphGelly {
    public static void main(String[] args) throws Exception {
        final ExecutionEnvironment env = ExecutionEnvironment.getExecutionEnvironment();
        final ParameterTool params = ParameterTool.fromArgs(args);
        env.getConfig().setGlobalJobParameters(params);

        if (!params.has("vertices") || !params.has("edges")) {
            buildGraph(env);
            return;
        }

        DataSource<Tuple3<Long, String, Integer>> vertex = env
                .readCsvFile(params.get("vertices"))
                .ignoreInvalidLines()
                .types(Long.class, String.class, Integer.class);
        DataSet<Tuple2<Long, Tuple2<String, Integer>>> vertexTuples = vertex
                .map(new MapFunction<Tuple3<Long, String, Integer>, Tuple2<Long, Tuple2<String, Integer>>>() {
                    @Override
                    public Tuple2<Long, Tuple2<String, Integer>> map(Tuple3<Long, String, Integer> value) throws Exception {
                        return Tuple2.of(value.f0, Tuple2.of(value.f1, value.f2));
                    }
                });

        DataSource<Tuple3<Long, Long, Integer>> edges = env
                .readCsvFile(params.get("edges"))
                .ignoreInvalidLines()
                .types(Long.class, Long.class, Integer.class);

        Graph<Long, Tuple2<String, Integer>, Integer> graph = Graph.fromTupleDataSet(vertexTuples, edges, env);

        System.out.println("\n--------------");
        System.out.println("Number of vertices: " + graph.numberOfVertices());
        System.out.println("Number of edges: " + graph.numberOfEdges());
        System.out.println("--------------\n");

        // env.execute("Graph");
    }

    private static void buildGraph(ExecutionEnvironment env) throws Exception {
        List<Tuple3<Long, String, Integer>> vertex = new ArrayList<>(26);
        for (int i = 0; i < 26; i++) {
            Long id = (long) i;
            String name = String.valueOf((char) (i + 'a'));
            Integer weight = (int) (Math.random() * 100);
            vertex.add(Tuple3.of(id, name, weight));
        }
        List<Tuple3<Long, Long, Integer>> edges = new LinkedList<>();
        for (Tuple3<Long, String, Integer> v1: vertex) {
            for (Tuple3<Long, String, Integer> v2: vertex) {
                if (Math.random() > 0.8) {
                    Integer weight = (int) (Math.random() * 100);
                    edges.add(Tuple3.of(v1.f0, v2.f0, weight));
                }
            }
        }

        env.fromCollection(vertex).writeAsCsv("graph-vertices.csv", FileSystem.WriteMode.OVERWRITE);
        env.fromCollection(edges).writeAsCsv("graph-edges.csv", FileSystem.WriteMode.OVERWRITE);

        env.execute("Build Graph");
    }
}
