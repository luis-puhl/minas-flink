package br.ufscar.dc.gsdr.mfog;

import br.ufscar.dc.gsdr.mfog.structs.LabeledExample;
import br.ufscar.dc.gsdr.mfog.util.Logger;
import br.ufscar.dc.gsdr.mfog.util.MfogManager;
import br.ufscar.dc.gsdr.mfog.util.Try;

import java.io.*;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Queue;
import java.util.concurrent.ConcurrentLinkedQueue;

public class SinkFog {
    static Logger LOG = Logger.getLogger("SinkFog");

    public static void main(String[] args) throws IOException, InterruptedException {
        ServerSocket serverSocket = new ServerSocket(MfogManager.SINK_MODULE_TEST_PORT);
        LOG.info("server ready on " + serverSocket.getInetAddress() + ":" + serverSocket.getLocalPort());
        //
        Socket classifierSocket = serverSocket.accept();
        classifierSocket.shutdownOutput();
        InputStream classifierStream = classifierSocket.getInputStream();
        LOG.info("connected to classifier");
        long startReceive = System.currentTimeMillis();
        //
        LOG.info("connecting to " + MfogManager.SERVICES_HOSTNAME + ":" + MfogManager.SOURCE_EVALUATE_DATA_PORT);
        Socket sourceSocket = new Socket(InetAddress.getByName(MfogManager.SERVICES_HOSTNAME), MfogManager.SOURCE_EVALUATE_DATA_PORT);
        LOG.info("connected to source");
        InputStream sourceStream = sourceSocket.getInputStream();
        //
        // {"label":"model latency=0","point":{"time":1587927915123,"id":0,"value":[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]}}
        Iterator<String> classifierIterator = new BufferedReader(new InputStreamReader(classifierStream)).lines().iterator();
        Iterator<String> sourceIterator = new BufferedReader(new InputStreamReader(sourceStream)).lines().iterator();
        List<LabeledExample> classifierBuffer = new ArrayList<>(100);
        List<LabeledExample> sourceBuffer = new ArrayList<>(100);
        long i = 0;
        long matches = 0;
        long lastCheck = 0;
        long sentTime = System.currentTimeMillis();
        while (classifierIterator.hasNext() || sourceIterator.hasNext() || classifierBuffer.size() > 0 || sourceBuffer.size() > 0) {
            while (classifierStream.available() > 10 && classifierIterator.hasNext()) {
                // LOG.info("classifier RCV");
                classifierBuffer.add(LabeledExample.fromJson(classifierIterator.next()));
            }
            while (sourceStream.available() > 10 && sourceIterator.hasNext()) {
                // LOG.info("source RCV");
                sourceBuffer.add(LabeledExample.fromJson(sourceIterator.next()));
            }
            for (LabeledExample classified : classifierBuffer) {
                for (LabeledExample source : sourceBuffer) {
                    if (classified.point.id == source.point.id) {
                        if (classified.label.equals(source.label)) {
                            matches++;
                        }
                        i++;
                        source.point.id = -1;
                        classified.point.id = -1;
                        break;
                    }
                }
            }
            classifierBuffer.removeIf(l -> l.point.id == -1);
            sourceBuffer.removeIf(l -> l.point.id == -1);
            if (System.currentTimeMillis() - sentTime > 1000) {
                if (lastCheck == i || i == 0) {
                    LOG.info("Strike");
                    Thread.sleep(5000);
                    continue;
                }
                lastCheck = i;
                sentTime = System.currentTimeMillis();
                String speed = ((int) (i / ((System.currentTimeMillis() - startReceive) * 10e-4))) + " i/s";
                LOG.info("i=" + i + " cls=" + classifierBuffer.size() + " src=" + sourceBuffer.size() + " " + speed);
            }
            // Thread.sleep(10);
        }
        LOG.info("received " + i + " items in " + (System.currentTimeMillis() - startReceive) * 10e-4 + "s");
        if (i != 0) {
            LOG.info("i=" + i + " matches=" + matches + " misses=" + (100 * (i - matches) / i) + "%");
        }
        classifierSocket.close();
        sourceSocket.close();
        serverSocket.close();
    }
}
