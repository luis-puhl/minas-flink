package br.ufscar.dc.gsdr.mfog;

import br.ufscar.dc.gsdr.mfog.structs.Cluster;
import br.ufscar.dc.gsdr.mfog.structs.Point;
import br.ufscar.dc.gsdr.mfog.util.MfogManager;
import br.ufscar.dc.gsdr.mfog.util.ModelStoreAkka;
import br.ufscar.dc.gsdr.mfog.util.Try;

import java.io.*;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.Iterator;
import java.util.logging.Logger;
import java.util.stream.Stream;

public class SourceKyoto {
    static final Logger LOG = Logger.getLogger(SourceKyoto.class.getName());
    public static void main(String[] args) throws InterruptedException {
        String training = "kyoto_binario_binarized_offline_1class_fold1_ini";
        String test = "kyoto_binario_binarized_offline_1class_fold1_onl";

        Thread trainingThread = new Thread(() -> SourceKyoto.sender(training, MfogManager.SOURCE_TRAINING_DATA_PORT, true));
        trainingThread.start();

        Thread testThread = new Thread(() -> SourceKyoto.sender(test, MfogManager.SOURCE_TEST_DATA_PORT, false));
        testThread.start();

        Thread evaluationThread = new Thread(() -> SourceKyoto.sender(test, MfogManager.SOURCE_EVALUATE_DATA_PORT, true));
        evaluationThread.start();

        trainingThread.join();
        testThread.join();
    }

    static void sender(String file, int port, boolean sendLabel) {
        String senderType = sendLabel ? "training": "test";
        ServerSocket senderServer;
        if ((senderServer = Try.apply(() -> new ServerSocket(port)).get) == null) return;
        //
        LOG.info("Sender ready " + senderType);
        for (int i = 0; i < 3; i++) {
            Socket socket;
            if ((socket = Try.apply(senderServer::accept).get) == null) break;
            long start = System.currentTimeMillis();
            LOG.info("sender connected " + senderType);
            //
            String path = "datasets" + File.separator + "kyoto-bin" + File.separator + file;
            FileReader fr;
            if ((fr = Try.apply(() -> new FileReader(path)).get) == null) {
                Try.apply(socket::close);
                break;
            }
            Stream<String> in = new BufferedReader(fr).lines();
            //
            OutputStream outputStream;
            if ((outputStream = Try.apply(socket::getOutputStream).get) == null) {
                Try.apply(socket::close);
                break;
            }
            PrintStream out = new PrintStream(outputStream);
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
