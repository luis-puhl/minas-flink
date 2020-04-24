package br.ufscar.dc.gsdr.mfog;

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
                () -> new BufferedReader(new FileReader(basePath + file)).lines()
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
        LabeledPoint labeledPoint = new LabeledPoint(idGenerator, line).invoke();
        String prefix = sendLabel ? labeledPoint.label + ">" : "";
        return prefix + labeledPoint.point.json().toString();
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

    static class LabeledPoint {
        private Iterator<Integer> idGenerator;
        private String line;
        public String label;
        public Point point;

        public LabeledPoint(Iterator<Integer> idGenerator, String line) {
            this.idGenerator = idGenerator;
            this.line = line;
        }
        public LabeledPoint invoke() {
            // 0.0,0.0,0.0,0.0,0.0,0.0,0.4,0.0,0.0,0.0,0.0,0.0,1,0,0,0,0,0,0,0,1,0,N
            String[] lineSplit = line.split(",");
            double[] doubles = new double[lineSplit.length -1];
            for (int j = 0; j < doubles.length -1; j++) {
                doubles[j] = Double.parseDouble(lineSplit[j]);
            }
            label = lineSplit[lineSplit.length - 1];
            point = Point.apply(idGenerator.next(), doubles);
            return this;
        }
    }
}
