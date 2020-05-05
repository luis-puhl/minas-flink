package br.ufscar.dc.gsdr.mfog;

import br.ufscar.dc.gsdr.mfog.structs.Cluster;
import br.ufscar.dc.gsdr.mfog.util.Logger;
import br.ufscar.dc.gsdr.mfog.util.MfogManager;
import br.ufscar.dc.gsdr.mfog.util.TCP;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

public class ModelStore {

    public static void main(String[] args) throws IOException {
        Logger log = Logger.getLogger(ModelStore.class);
        List<Cluster> model = new ArrayList<>(100);
        //
        TCP<Cluster> util = new TCP<>(Cluster.class, new Cluster(), false, ModelStore.class);
        util.server(MfogManager.MODEL_STORE_PORT);
        log.info("server ready on " + util.serverSocket.getInetAddress() + ":" + util.serverSocket.getLocalPort());
        //
        util.serverAccept();
        log.info("Receiving");
        while (util.hasNext()) {
            try {
                Cluster cluster = util.receive(new Cluster());
                model.add(cluster);
            } catch (java.io.EOFException e) {
                break;
            }
        }
        util.closeSocket();
        //
        util.serverAccept();
        log.info("Sending to classifier");
        for (Cluster cluster : model) {
            if (!util.isConnected()) {
                util.serverAccept();
                log.info("Reconnect");
            }
            util.send(cluster);
        }
        util.flush();
        util.closeSocket();
        util.closeServer();
        log.info("done");
    }
}
