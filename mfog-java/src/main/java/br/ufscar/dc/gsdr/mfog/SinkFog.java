package br.ufscar.dc.gsdr.mfog;

import br.ufscar.dc.gsdr.mfog.structs.Point;
import br.ufscar.dc.gsdr.mfog.util.Logger;
import br.ufscar.dc.gsdr.mfog.util.MfogManager;
import br.ufscar.dc.gsdr.mfog.util.TcpUtil;

import java.io.IOException;
import java.util.Queue;
import java.util.concurrent.ConcurrentLinkedQueue;

public class SinkFog {
    static Logger LOG = Logger.getLogger("SinkFog");

    static class LabeledExample {
        public Point point;
        public String label;

        public static LabeledExample fromClassifier(String s) {
            return new LabeledExample("label", Point.zero(22));
        }
        public static LabeledExample fromSource(String s) {
            return new LabeledExample("label", Point.zero(22));
        }

        public LabeledExample(String label, Point point) {
            this.point = point;
            this.label = label;
        }
    }

    public static void main(String[] args) throws IOException {
        Queue<LabeledExample> fromClassifier = new ConcurrentLinkedQueue<>();
        TcpUtil<LabeledExample> fromClassifierTcp = new TcpUtil<>(
                "sink from classifier",
                MfogManager.SINK_MODULE_TEST_PORT,
                null,
                LabeledExample::fromClassifier,
                fromClassifier
        );
        fromClassifierTcp.waitBeforeRcv = 129987;
        fromClassifierTcp.waitBetweenRcv = 129987 / 653457;
        fromClassifierTcp.server();
        LOG.info("fromClassifier.size()=" + fromClassifier.size());
        //
        Queue<LabeledExample> fromSource = new ConcurrentLinkedQueue<>();
        TcpUtil<LabeledExample> fromSourceTcp = new TcpUtil<>(
                "sink from classifier",
                MfogManager.SOURCE_EVALUATE_DATA_PORT,
                null,
                LabeledExample::fromSource,
                fromSource
        );
        fromSourceTcp.client();
        //
    }

    /*
    void any() {
        ServerSocket senderServer;
        if ((senderServer = Try.apply(() -> new ServerSocket(MfogManager.SINK_MODULE_TEST_PORT)).get) == null) return;
        //
        LOG.info("Test receiver ready");
        for (int i = 0; i < 3; i++) {
            Socket socket;
            if ((socket = Try.apply(senderServer::accept).get) == null) break;
            long start = System.currentTimeMillis();

            LOG.info("Test receiver connected ");
            //
            InetAddress addr;
            if ((addr = Try.apply(() -> InetAddress.getByName(MfogManager.SERVICES_HOSTNAME)).get) == null) break;
            Socket evaluateSocket;
            if ((evaluateSocket = Try.apply(() -> new Socket(addr, MfogManager.SOURCE_EVALUATE_DATA_PORT)).get) == null) break;
            OutputStream evaluateStream;
            if ((evaluateStream = Try.apply(evaluateSocket::getOutputStream).get) == null) {
                Try.apply(evaluateSocket::close);
                break;
            }
            PrintStream out = new PrintStream(evaluateStream);
            //
            long id = 0;
            Iterator<String> iterator = in.iterator();
            while (iterator.hasNext()) {
                String line = iterator.next();
                // 0.0,0.0,0.0,0.0,0.0,0.0,0.4,0.0,0.0,0.0,0.0,0.0,1,0,0,0,0,0,0,0,1,0,N
                String[] lineSplit = line.split(",");
                double[] doubles = new double[lineSplit.length];
                for (int j = 0; j < doubles.length; j++) {
                    doubles[j] = Double.parseDouble(lineSplit[j]);
                }
                String label = lineSplit[lineSplit.length-1];
                Point p = Point.apply(id, doubles);
                if (sendLabel) {
                    out.println(label + ">" + p.json().toString());
                } else {
                    out.println(p.json().toString());
                }
                id++;
            }
            in.forEach(out::println);
            out.flush();
            Try.apply(socket::close);
            LOG.info("sent " + id + " items in " + (System.currentTimeMillis() - start) * 10e-4 + "s " + senderType);
        }
        Try.apply(senderServer::close);
    }
     */
}
