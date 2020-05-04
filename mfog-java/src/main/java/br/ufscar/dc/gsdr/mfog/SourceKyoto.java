package br.ufscar.dc.gsdr.mfog;

import br.ufscar.dc.gsdr.mfog.structs.LabeledExample;
import br.ufscar.dc.gsdr.mfog.structs.Point;
import br.ufscar.dc.gsdr.mfog.util.Logger;
import br.ufscar.dc.gsdr.mfog.util.MfogManager;
import br.ufscar.dc.gsdr.mfog.util.ServerClient;
import br.ufscar.dc.gsdr.mfog.util.TcpUtil;
import org.apache.commons.lang3.SerializationUtils;

import java.io.*;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.*;
import java.util.concurrent.*;
import java.util.stream.Collectors;
import java.util.stream.Stream;

public class SourceKyoto {
    static final String training = "kyoto_binario_binarized_offline_1class_fold1_ini";
    static final String test = "kyoto_binario_binarized_offline_1class_fold1_onl";
    static final String basePath = "datasets" + File.separator + "kyoto-bin" + File.separator;
    static final Logger LOG = Logger.getLogger(SourceKyoto.class);

    public static void main(String[] args) throws InterruptedException, IOException {
        Thread trainingServer = new Thread(() -> {
            try {
                IdGenerator idGenerator = new IdGenerator();
                Iterator<LabeledExample> iterator = new BufferedReader(
                        new FileReader(basePath + training)
                ).lines().map(line -> LabeledExample.fromKyotoCSV(idGenerator.next(), line)).iterator();
                //
                ServerClient<LabeledExample> server = new ServerClient<>(LabeledExample.class, new LabeledExample(), false, SourceKyoto.class);
                server.server(MfogManager.SOURCE_TRAINING_DATA_PORT);
                server.serverAccept();
                while (server.isConnected() && iterator.hasNext()) {
                    server.send(iterator.next());
                }
                server.flush();
                server.closeSocket();
                server.closeServer();
            } catch (IOException e) {
                e.printStackTrace();
            }
        });
        trainingServer.start();

        IdGenerator idGenerator = new IdGenerator();
        Iterator<LabeledExample> iterator = new BufferedReader(new FileReader(basePath + test)).lines()
            .limit(1000)
            .map(line -> LabeledExample.fromKyotoCSV(idGenerator.next(), line))
            .collect(Collectors.toList()).iterator();

        Queue<LabeledExample> testQueue = new LinkedList<>();
        Queue<LabeledExample> evaluationQueue = new LinkedList<>();

        //
        class EvaluatorRunnable implements Runnable {
            volatile boolean isRunning = true;
            @Override
            public void run() {
                final Logger log = Logger.getLogger(EvaluatorRunnable.class);
                try {
                    ServerClient<LabeledExample> evaluator = new ServerClient<>(LabeledExample.class, new LabeledExample(), false, EvaluatorRunnable.class);
                    evaluator.server(MfogManager.SOURCE_EVALUATE_DATA_PORT);
                    log.info("Evaluator ready");
                    while (isRunning && iterator.hasNext() || !evaluationQueue.isEmpty()) {
                        if (evaluator.isConnected()) {
                            if (!testQueue.isEmpty()) {
                                evaluator.send(testQueue.poll());
                            } else if (iterator.hasNext()) {
                                LabeledExample next = iterator.next();
                                evaluator.send(next);
                                evaluationQueue.add(next);
                            }
                        } else if (isRunning) {
                            log.info("Evaluator Accept");
                            evaluator.serverAccept();
                        }
                    }
                    evaluator.flush();
                    evaluator.closeSocket();
                    evaluator.closeServer();
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
            public void cancel() {
                isRunning = false;
            }
        }
        EvaluatorRunnable evaluatorRunnable = new EvaluatorRunnable();
        Thread evaluatorThread = new Thread(evaluatorRunnable);
        evaluatorThread.start();
        //
        ServerClient<Point> classifier = new ServerClient<>(Point.class, new Point(), false, SourceKyoto.class);
        classifier.server(MfogManager.SOURCE_TEST_DATA_PORT);
        LOG.info("Classifier Ready");
        while (iterator.hasNext() || !testQueue.isEmpty()){
            if (classifier.isConnected()) {
                if (!testQueue.isEmpty()) {
                    classifier.send(testQueue.poll().point);
                } else if (iterator.hasNext()) {
                    LabeledExample next = iterator.next();
                    classifier.send(next.point);
                    evaluationQueue.add(next);
                }
            } else {
                LOG.info("Classifier Accept");
                classifier.serverAccept();
            }
        }
        classifier.flush();
        classifier.closeSocket();
        classifier.closeServer();

        evaluatorRunnable.cancel();
        evaluatorThread.interrupt();
        evaluatorThread.join();
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
