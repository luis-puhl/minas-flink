package br.ufscar.dc.gsdr.mfog.kiss;

import org.apache.flink.configuration.Configuration;
import org.apache.flink.streaming.api.datastream.DataStreamSource;
import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment;
import org.apache.flink.streaming.api.functions.sink.RichSinkFunction;
import org.apache.flink.streaming.api.functions.sink.SinkFunction;
import org.apache.flink.streaming.api.functions.source.SourceFunction;
import org.slf4j.LoggerFactory;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.LinkedList;
import java.util.List;

public class ServerSourceKISS {
    static final org.slf4j.Logger log = LoggerFactory.getLogger(ServerSourceKISS.class);
    static String hostname = "localhost";
    static int port = 8899;

    public static void main(String[] args) throws Exception {
        StreamExecutionEnvironment env = StreamExecutionEnvironment.getExecutionEnvironment();
        // ExecutionConfig config = env.getConfig();
        // config.addDefaultKryoSerializer(Cluster.class, new Cluster());

        DataStreamSource<Pogo> pogoSource = env.addSource(new SourceFunction<Pogo>() {
            volatile Boolean running = true;
            ServerSocket serverSocket;
            List<Socket> connections = new LinkedList<>();
            List<Thread> connectionRunners = new LinkedList<>();

            @Override
            public void run(final SourceContext<Pogo> ctx) throws Exception {
                serverSocket = new ServerSocket(port);
                log.info("Listening on {}", serverSocket.getInetAddress());
                while (running) {
                    final Socket socket = serverSocket.accept();
                    log.info("Connected to {}", socket.getRemoteSocketAddress());
                    connections.add(socket);
                    Thread thread = new Thread(() -> {
                        try {
                            final DataOutputStream out = new DataOutputStream(socket.getOutputStream());
                            final DataInputStream in = new DataInputStream(socket.getInputStream());
                            int n = in.readInt();
                            for (int i = 0; i < n && socket.isConnected(); i++) {
                                Pogo read = Pogo.read(in);
                                ctx.collect(read);
                            }
                        } catch (IOException e) {
                            log.error("Failed to initialize connection", e);
                        }
                    });
                    thread.start();
                    connectionRunners.add(thread);
                }
                serverSocket.close();
            }

            @Override
            public void cancel() {
                running = false;
            }
        });
        pogoSource.print();

        DataStreamSource<Pogo> pogoElements = env.fromElements(new Pogo(0, "a", 0.1f, 0.2f),
            new Pogo(1, "b", 0.1f, 0.2f), new Pogo(2, "c", 0.1f, 0.2f)
        );

        pogoElements.addSink(new RichSinkFunction<Pogo>() {
            @Override
            public void open(Configuration parameters) throws Exception {
                super.open(parameters);
            }

            @Override
            public void invoke(Pogo value, Context context) throws Exception {
                //
            }

            @Override
            protected void finalize() throws Throwable {
                super.finalize();
            }
        });

        env.execute(ServerSourceKISS.class.getName());
    }

    public static class Pogo {
        public int id;
        public float[] point;
        public String label;

        static public Pogo read(DataInputStream in) throws IOException {
            Pogo pogo = new Pogo();
            pogo.id = in.readInt();
            final int length = in.readInt();
            pogo.point = new float[length];
            for (int i = 0; i < length; i++) {
                pogo.point[i] = in.readFloat();
            }
            pogo.label = in.readUTF();
            return pogo;
        }

        public Pogo() {}
        public Pogo(int id, String label, float... point) {
            this.id = id;
            this.point = point;
            this.label = label;
        }

        public void write(DataOutputStream out) throws IOException {
            out.writeLong(this.id);
            out.writeInt(this.point.length);
            for (int i = 0; i < this.point.length; i++) {
                out.writeFloat(this.point[i]);
            }
            out.writeUTF(this.label);
        }
    }
}
