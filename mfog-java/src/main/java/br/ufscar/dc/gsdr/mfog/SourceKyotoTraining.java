package br.ufscar.dc.gsdr.mfog;

import br.ufscar.dc.gsdr.mfog.structs.LabeledExample;
import br.ufscar.dc.gsdr.mfog.structs.Point;
import br.ufscar.dc.gsdr.mfog.util.Logger;
import br.ufscar.dc.gsdr.mfog.util.MfogManager;
import br.ufscar.dc.gsdr.mfog.util.TCP;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.Queue;
import java.util.stream.Collectors;

public class SourceKyotoTraining {
    static final String training = "kyoto_binario_binarized_offline_1class_fold1_ini";
    static final String test = "kyoto_binario_binarized_offline_1class_fold1_onl";
    static final String basePath = "datasets" + File.separator + "kyoto-bin" + File.separator;
    static final Logger LOG = Logger.getLogger(SourceKyotoTraining.class);

    public static void main(String[] args) throws InterruptedException, IOException {
        IdGenerator idGenerator = new IdGenerator();
        Iterator<LabeledExample> iterator = new BufferedReader(
                new FileReader(basePath + training)
        ).lines().map(line -> LabeledExample.fromKyotoCSV(idGenerator.next(), line)).iterator();
        //
        TCP<LabeledExample> server = new TCP<>(LabeledExample.class, new LabeledExample(), SourceKyotoTraining.class);
        server.server(MfogManager.SOURCE_TRAINING_DATA_PORT);
        server.serverAccept();
        while (server.isConnected() && iterator.hasNext()) {
            server.send(iterator.next());
        }
        server.flush();
        server.close();
        server.closeServer();
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
