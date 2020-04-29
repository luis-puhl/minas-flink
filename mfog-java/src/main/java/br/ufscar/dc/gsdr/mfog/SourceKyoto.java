package br.ufscar.dc.gsdr.mfog;

import br.ufscar.dc.gsdr.mfog.structs.LabeledExample;
import br.ufscar.dc.gsdr.mfog.util.Logger;
import br.ufscar.dc.gsdr.mfog.util.MfogManager;
import br.ufscar.dc.gsdr.mfog.util.TcpUtil;

import java.io.*;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.stream.Stream;

public class SourceKyoto {
    static final String training = "kyoto_binario_binarized_offline_1class_fold1_ini";
    static final String test = "kyoto_binario_binarized_offline_1class_fold1_onl";
    static final String basePath = "datasets" + File.separator + "kyoto-bin" + File.separator;
    static final Logger LOG = Logger.getLogger("SourceKyoto");

    public static void main(String[] args) throws InterruptedException, IOException {
        TcpUtil<String> tcpTraining = new TcpUtil<>(
                "Source Kyoto Training",
                MfogManager.SOURCE_TRAINING_DATA_PORT,
                () -> {
                    IdGenerator idGenerator = new IdGenerator();
                    return new BufferedReader(new FileReader(basePath + training)).lines()
                                   .map(line -> LabeledExample.fromKyotoCSV(idGenerator.next(), line).json().toString()).iterator();
                },
                null,
                null
        );
        Thread trainingThread = new Thread(tcpTraining::server);
        trainingThread.start();

        ServerSocket classifierServerSocket = new ServerSocket(MfogManager.SOURCE_TEST_DATA_PORT);
        // ServerSocket sinkServerSocket = new ServerSocket(MfogManager.SOURCE_EVALUATE_DATA_PORT);
        LOG.info("Test/Eval ready");
        List<Thread> evaluationThreads = new ArrayList<>(10);
        for (int i = 0; i < 10; i++) {
            final Socket classifierSocket = classifierServerSocket.accept();
            LOG.info("Test connected, waiting for sink");
            // final Socket sinkSocket = sinkServerSocket.accept();
            LOG.info("Eval connected");
            Thread evaluationThread = new Thread(() -> {
                long startTime = System.currentTimeMillis();
                try {
                    BufferedReader testReader = new BufferedReader(new FileReader(basePath + test));
                    IdGenerator idGenerator = new IdGenerator();
                    Stream<LabeledExample> labeledExampleStream = testReader.lines().map(
                            line -> LabeledExample.fromKyotoCSV(idGenerator.next(), line)
                    );
                    //
                    OutputStream classifierStream = classifierSocket.getOutputStream();
                    classifierSocket.shutdownInput();
                    // OutputStream sinkStream = sinkSocket.getOutputStream();
                    PrintStream classifier = new PrintStream(classifierStream);
                    // PrintStream sink = new PrintStream(sinkStream);
                    //
                    long sentTime = System.currentTimeMillis();
                    Iterator<LabeledExample> iterator = labeledExampleStream.iterator();
                    long sent = 0;
                    while (iterator.hasNext()) {
                        LabeledExample labeledExample = iterator.next();
                        classifier.println(labeledExample.point.json());
                        classifier.flush();
                        // sink.println(labeledExample.json());
                        // sink.flush();
                        sent++;
                        if (System.currentTimeMillis() - sentTime > TcpUtil.REPORT_INTERVAL) {
                            String speed = ((int) (sent / ((System.currentTimeMillis() - startTime) * 10e-4))) + " i/s";
                            sentTime = System.currentTimeMillis();
                            LOG.info("sent=" + sent + " " + speed);
                        }
                    }
                    classifier.flush();
                    // sink.flush();
                } catch (IOException e) {
                    LOG.error(e);
                } finally {
                    try {
                        classifierSocket.close();
                        // sinkSocket.close();
                    } catch (IOException e) {
                        LOG.error(e);
                    }
                }
            });
            evaluationThread.start();
            evaluationThreads.add(evaluationThread);
        }
        for (Thread evaluationThread : evaluationThreads) {
            evaluationThread.join();
        }
        trainingThread.join();
        LOG.info("done");
    }

    static class IdGenerator implements Iterator<Integer> {
        int id = 0;

        @Override
        public boolean hasNext() {
            return true;
        }

        @Override
        public Integer next() {
            return id++;
        }
    }
}
