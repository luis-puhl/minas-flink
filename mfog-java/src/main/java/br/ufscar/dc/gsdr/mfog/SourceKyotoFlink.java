package br.ufscar.dc.gsdr.mfog;

import br.ufscar.dc.gsdr.mfog.kiss.KryoKissServer;
import br.ufscar.dc.gsdr.mfog.structs.*;
import br.ufscar.dc.gsdr.mfog.util.IdGenerator;
import br.ufscar.dc.gsdr.mfog.util.MfogManager;
import br.ufscar.dc.gsdr.mfog.util.TimeItConnection;
import com.esotericsoftware.kryonet.Connection;
import com.esotericsoftware.kryonet.Listener;
import com.esotericsoftware.kryonet.Server;
import org.apache.flink.api.common.ExecutionConfig;
import org.apache.flink.streaming.api.datastream.SingleOutputStreamOperator;
import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment;
import org.slf4j.LoggerFactory;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.util.Iterator;
import java.util.List;
import java.util.stream.Collectors;
import java.util.stream.Stream;

public class SourceKyotoFlink {
    static final org.slf4j.Logger log = LoggerFactory.getLogger(SourceKyotoFlink.class);

    public static void main(String[] args) throws Exception {
        StreamExecutionEnvironment env = StreamExecutionEnvironment.getExecutionEnvironment();
        ExecutionConfig config = env.getConfig();
        config.addDefaultKryoSerializer(Cluster.class, new Cluster());
        config.addDefaultKryoSerializer(Point.class, new Point());
        config.addDefaultKryoSerializer(LabeledExample.class, new LabeledExample());
        config.addDefaultKryoSerializer(Model.class, new Model());
        IdGenerator idGenerator = new IdGenerator();

        SingleOutputStreamOperator<Point> examples = env
            .readTextFile(MfogManager.Kyoto.basePath + MfogManager.Kyoto.test)
            .map(line -> LabeledExample.fromKyotoCSV(idGenerator.next(), line).point);

        examples.addSink(new KryoNetServerSink<>(Point.class, MfogManager.SOURCE_TEST_DATA_PORT));

        env.execute();
        log.info("done");
    }

    void trainingSource() throws IOException {
        IdGenerator idGenerator = new IdGenerator();
        Stream<LabeledExample> labeledExampleStream = new BufferedReader(
            new FileReader(MfogManager.Kyoto.basePath + MfogManager.Kyoto.training)).lines()
            .map(line -> LabeledExample.fromKyotoCSV(idGenerator.next(), line));
        Iterator<LabeledExample> iterator = labeledExampleStream.iterator();
        //
        Server server = new Server();
        Serializers.registerMfogStructs(server.getKryo());
        server.addListener(new Listener() {
            @Override
            public void received(Connection connection, Object message) {
                if (message instanceof Message) {
                    Message msg = (Message) message;
                    if (msg.isDone()) {
                        log.info("done");
                        connection.close();
                    }
                    if (msg.isReceive()) {
                        for (; iterator.hasNext(); ) {
                            LabeledExample labeledExample = iterator.next();
                            labeledExample.point.time = System.currentTimeMillis();
                            connection.sendTCP(labeledExample);
                        }
                    }
                }
            }

            @Override
            public void disconnected(Connection conn) {
                TimeItConnection connection = (TimeItConnection) conn;
                log.info(connection.timeIt.finish(labeledExampleStream.count()));
            }
        });
        server.bind(MfogManager.SOURCE_TEST_DATA_PORT);
        server.run();
    }
}
