package br.ufscar.dc.gsdr.mfog;

import br.ufscar.dc.gsdr.mfog.structs.LabeledExample;
import br.ufscar.dc.gsdr.mfog.util.Logger;
import br.ufscar.dc.gsdr.mfog.util.MfogManager;
import br.ufscar.dc.gsdr.mfog.util.TCP;

import java.io.IOException;
import java.util.LinkedList;
import java.util.List;

public class SinkFog {
    static Logger LOG = Logger.getLogger(SinkFog.class);

    public static void main(String[] args) throws IOException, InterruptedException {
        TCP<LabeledExample> classifier = new TCP<>(LabeledExample.class, new LabeledExample(), SinkFog.class);
        classifier.server(MfogManager.SINK_MODULE_TEST_PORT);
        LOG.info(
            "classifier ready on " + classifier.serverSocket.getInetAddress() + ":" + classifier.serverSocket.getLocalPort());
        classifier.serverAccept();
        LOG.info(
            "connected to classifier, connecting to " + MfogManager.SERVICES_HOSTNAME + ":" + MfogManager.SOURCE_EVALUATE_DATA_PORT);
        TCP<LabeledExample> source = new TCP<>(LabeledExample.class, new LabeledExample(), SinkFog.class);
        source.client(MfogManager.SERVICES_HOSTNAME, MfogManager.SOURCE_EVALUATE_DATA_PORT);
        LOG.info("connected to source");

        List<LabeledExample> fromClassifier = new LinkedList<>();
        List<LabeledExample> fromSource = new LinkedList<>();
        List<LabeledExample> toRemove = new LinkedList<>();

        long i = 0;
        long matches = 0;
        while (classifier.hasNext() || source.hasNext()) {
            if (classifier.hasNext()) {
                fromClassifier.add(classifier.next());
            }
            if (source.hasNext()) {
                fromSource.add(source.next());
            }
            if (fromClassifier.isEmpty() || fromSource.isEmpty()) {
                continue;
            }
            for (LabeledExample a : fromClassifier) {
                for (LabeledExample b : fromSource) {
                    if (a.point.id == b.point.id) {
                        toRemove.add(a);
                        i++;
                        if (a.label.equals(b.label)) {
                            matches++;
                        }
                    }
                }
            }
            fromClassifier.removeAll(toRemove);
            fromSource.removeAll(toRemove);
            toRemove.clear();
        }
        if (i != 0) {
            LOG.info("i=" + i + " matches=" + matches + " misses=" + (100 * (i - matches) / i) + "%");
        }
        classifier.close();
        classifier.closeServer();
        source.close();
        LOG.info("done");
    }
}
