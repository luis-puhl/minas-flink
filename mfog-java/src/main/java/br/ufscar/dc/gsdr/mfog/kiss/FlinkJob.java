package br.ufscar.dc.gsdr.mfog.kiss;

import br.ufscar.dc.gsdr.mfog.KryoNetClientSink;
import br.ufscar.dc.gsdr.mfog.KryoNetClientSource;
import org.apache.flink.api.common.typeinfo.TypeInformation;
import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment;

public class FlinkJob {
    public static void main(String[] args) throws Exception {
        StreamExecutionEnvironment env = StreamExecutionEnvironment.getExecutionEnvironment();
        env
            .addSource(
                new KryoNetClientSource<>(Integer.class, "localhost", 8888),
                "KryoNet model",
                TypeInformation.of(Integer.class)
            )
            .map(i -> i + 1)
            .addSink(new KryoNetClientSink<>(Integer.class, "localhost", 8080)).name("KryoNet Sink");
        env.execute("Keep It Simple, Stupid");
    }
}
