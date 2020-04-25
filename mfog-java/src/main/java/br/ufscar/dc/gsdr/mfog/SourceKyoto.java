package br.ufscar.dc.gsdr.mfog;

import br.ufscar.dc.gsdr.mfog.structs.LabeledExample;
import br.ufscar.dc.gsdr.mfog.structs.Point;
import br.ufscar.dc.gsdr.mfog.util.MfogManager;
import br.ufscar.dc.gsdr.mfog.util.TcpUtil;

import java.io.*;
import java.util.Arrays;
import java.util.Iterator;

public class SourceKyoto {
    static final String training = "kyoto_binario_binarized_offline_1class_fold1_ini";
    static final String test = "kyoto_binario_binarized_offline_1class_fold1_onl";
    static final String basePath = "datasets" + File.separator + "kyoto-bin" + File.separator;

    static TcpUtil<String> tpcFactory(String serviceName, int port, String file, boolean sendLabel) {
        return new TcpUtil<>(
                serviceName,
                port,
                () -> new BufferedReader(new FileReader(basePath + file))
                    .lines()
                    .limit(1000)
                    .map(line -> transform(sendLabel, new IdGenerator(), line)),
                null,
                null
        );
    }
    public static void main(String[] args) throws InterruptedException {
        TcpUtil<String> tcpTraining = tpcFactory(
                "Source Kyoto Training",
                MfogManager.SOURCE_TRAINING_DATA_PORT,
                training,
                true
        );
        Thread trainingThread = new Thread(tcpTraining::server);
        trainingThread.start();

        TcpUtil<String> tcpTest = tpcFactory(
                "Source Kyoto Test",
                MfogManager.SOURCE_TEST_DATA_PORT,
                test,
                false
        );
        Thread testThread = new Thread(tcpTest::server);
        testThread.start();

        TcpUtil<String> tcpEvaluation = tpcFactory(
                "Source Kyoto Evaluate",
                MfogManager.SOURCE_EVALUATE_DATA_PORT,
                test,
                true
        );
        Thread evaluationThread = new Thread(tcpEvaluation::server);
        evaluationThread.start();

        trainingThread.join();
        testThread.join();
        evaluationThread.join();
    }

    public static String transform(boolean sendLabel, Iterator<Integer> idGenerator, String line) {
        LabeledExample labeledPoint = LabeledExample.fromKyotoCSV(idGenerator.next(), line);
        if (sendLabel) {
            return labeledPoint.json().toString() + "\n";
        }
        return labeledPoint.point.json().toString() + "\n";
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
