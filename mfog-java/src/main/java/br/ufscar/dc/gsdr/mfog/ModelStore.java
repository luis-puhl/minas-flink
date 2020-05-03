package br.ufscar.dc.gsdr.mfog;

import br.ufscar.dc.gsdr.mfog.structs.Cluster;
import br.ufscar.dc.gsdr.mfog.util.Logger;
import br.ufscar.dc.gsdr.mfog.util.MfogManager;
import br.ufscar.dc.gsdr.mfog.util.ServerClient;
import br.ufscar.dc.gsdr.mfog.util.TcpUtil;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

public class ModelStore {

    public static void main(String[] args) throws IOException, InterruptedException {
        new ModelStore().detailed();
    }
    Logger log = Logger.getLogger(ModelStore.class);
    public void detailed() throws IOException {
        List<Cluster> model = new ArrayList<>(100);
        //
        ServerClient<Cluster> util = new ServerClient<>(Cluster.class, new Cluster(), false, ModelStore.class);
        util.server(MfogManager.MODEL_STORE_PORT);
        log.info("server ready on " + util.serverSocket.getInetAddress() + ":" + util.serverSocket.getLocalPort());
        long start = System.currentTimeMillis();
        //
        util.serverAccept();
        log.info("Receiving");
        long rcvTime =  System.currentTimeMillis();
        int rcv = 0;
        while (util.hasNext()) {
            Cluster cluster = util.next();
            if (cluster != null) {
                model.add(cluster);
            } else {
                break;
            }
            if (System.currentTimeMillis() - rcvTime > TcpUtil.REPORT_INTERVAL) {
                int speed = ((int) (rcv / ((System.currentTimeMillis() - rcvTime) * 10e-4)));
                rcvTime = System.currentTimeMillis();
                log.info("rcv=" + rcv + " " + speed + " i/s");
            }
        }
        //
        long sndTime = System.currentTimeMillis();
        int snd = 0;
        for (Cluster cluster : model) {
            util.serverAccept();
            log.info("Sending to classifier");
            if (util.isConnected()) {
                util.send(cluster);
                if (System.currentTimeMillis() - sndTime > TcpUtil.REPORT_INTERVAL) {
                    int speed = ((int) (snd / ((System.currentTimeMillis() - sndTime) * 10e-4)));
                    sndTime = System.currentTimeMillis();
                    log.info("snd=" + snd + " " + speed + " i/s");
                }
            }
        }
        util.flush();
        util.close();
        //
        log.info("total " + (rcv + snd) + " items in " + (System.currentTimeMillis() - start) * 10e-4 + "s");
        log.info("done");
    }
}
