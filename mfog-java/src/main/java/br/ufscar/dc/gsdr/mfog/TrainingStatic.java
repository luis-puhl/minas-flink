package br.ufscar.dc.gsdr.mfog;

import br.ufscar.dc.gsdr.mfog.structs.Cluster;
import br.ufscar.dc.gsdr.mfog.util.Logger;
import br.ufscar.dc.gsdr.mfog.util.MfogManager;
import br.ufscar.dc.gsdr.mfog.util.TCP;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.util.List;
import java.util.stream.Collectors;

public class TrainingStatic {
    public static void main(String[] args) throws Exception {
        final Logger LOG = Logger.getLogger(TrainingStatic.class);
        String path = "datasets" + File.separator + "models" + File.separator + "offline.csv";
        BufferedReader in = new BufferedReader(new FileReader(path));
        List<Cluster> model = in.lines().skip(1).limit(100).map(Cluster::fromMinasCsv).collect(Collectors.toList());
        //
        LOG.info("connecting to " + MfogManager.SERVICES_HOSTNAME + ":" + MfogManager.MODEL_STORE_PORT);
        TCP<Cluster> socket = new TCP<>(Cluster.class, new Cluster(), false, TrainingStatic.class);
        socket.client(MfogManager.SERVICES_HOSTNAME, MfogManager.MODEL_STORE_PORT);
        LOG.info("Sending");
        for (Cluster cluster : model) {
            if (!socket.isConnected()) {
                LOG.warn("Reconnecting");
                socket.client(MfogManager.SERVICES_HOSTNAME, MfogManager.MODEL_STORE_PORT);
            }
            socket.send(cluster);
        }
        socket.flush();
        socket.closeSocket();
        LOG.info("done");
    }
}
