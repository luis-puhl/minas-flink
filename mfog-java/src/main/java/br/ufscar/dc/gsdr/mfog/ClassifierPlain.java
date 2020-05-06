package br.ufscar.dc.gsdr.mfog;

import br.ufscar.dc.gsdr.mfog.structs.Cluster;
import br.ufscar.dc.gsdr.mfog.structs.LabeledExample;
import br.ufscar.dc.gsdr.mfog.structs.Point;
import br.ufscar.dc.gsdr.mfog.util.Logger;
import br.ufscar.dc.gsdr.mfog.util.MfogManager;

import java.io.BufferedInputStream;
import java.io.DataInputStream;
import java.io.IOException;
import java.net.InetAddress;
import java.net.Socket;
import java.util.ArrayList;
import java.util.List;

public class ClassifierPlain {
    public static void main(String[] args) {
        final Logger LOG = Logger.getLogger(ClassifierPlain.class);
        List<Cluster> model = new ArrayList<>(100);
        List<Point> exampleBuffer = new ArrayList<>(100);
        int clusterMinLength = Cluster.apply(0, Point.zero(22), 0, "").toBytes().length;
        try {
            InetAddress serviceHostname = InetAddress.getByName(MfogManager.SERVICES_HOSTNAME);
            Socket testSocket = new Socket(serviceHostname, MfogManager.SOURCE_TEST_DATA_PORT);
            DataInputStream testReader = new DataInputStream(new BufferedInputStream(testSocket.getInputStream()));
            try {
                Socket modelSocket = new Socket(serviceHostname, MfogManager.MODEL_STORE_PORT);
                DataInputStream modelReader = new DataInputStream(new BufferedInputStream(modelSocket.getInputStream()));
                while (testSocket.isConnected()) {
                    int testAvailable = testReader.available();
                    if (testAvailable < 0) {
                        break;
                    }
                    if (testAvailable >= 22) {
                        int length = testReader.readInt();
                        if (length > 0) {
                            byte[] message = new byte[length];
                            testReader.readFully(message, 0, message.length);
                            Point example = Point.zero(22);
                            exampleBuffer.add(example);
                            //
                            if (model.size() != 0) {
                                for (Point point : exampleBuffer) {
                                    double minDist = Double.MAX_VALUE;
                                    Cluster closest = null;
                                    for (Cluster cluster : model) {
                                        double dist = cluster.center.distance(point);
                                        if (dist < minDist) {
                                            minDist = dist;
                                            closest = cluster;
                                        }
                                    }
                                    assert closest != null;
                                    LabeledExample labeledExample = new LabeledExample(closest.label, point);
                                    System.out.println(labeledExample.label);
                                }
                            }
                        }
                    }
                    if (modelSocket.isConnected()) {
                        int modelAvailable = modelReader.available();
                        if (modelAvailable < 0) {
                            modelReader.close();
                            modelSocket.close();
                        }
                        if (modelAvailable >= clusterMinLength) {
                            int length = testReader.readInt();
                            if (length > 0) {
                                byte[] message = new byte[length];
                                testReader.readFully(message);
                                Cluster cluster = Cluster.fromBytes(message);
                                model.add(cluster);
                            }
                        }
                    }
                }
            } finally {
                testReader.close();
                testSocket.close();
            }
        } catch (IOException e) {
            LOG.error(e);
        }
    }
}
