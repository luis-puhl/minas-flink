package br.ufscar.dc.gsdr.mfog;

import br.ufscar.dc.gsdr.mfog.structs.Point;
import br.ufscar.dc.gsdr.mfog.util.MfogManager;
import br.ufscar.dc.gsdr.mfog.util.Try;

import java.io.*;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.logging.Logger;
import java.util.stream.Stream;

public class SinkFog {
    static final Logger LOG = Logger.getLogger(SinkFog.class.getName());
    public static void main(String[] args) {
        ServerSocket senderServer;
        if ((senderServer = Try.apply(() -> new ServerSocket(MfogManager.SINK_MODULE_TEST_PORT)).get) == null) return;
        //
        LOG.info("Test receiver ready");
        for (int i = 0; i < 3; i++) {
            Socket socket;
            if ((socket = Try.apply(senderServer::accept).get) == null) break;
            long start = System.currentTimeMillis();
            List<Point> toBeEvaluated = new ArrayList<>(100);
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
}
