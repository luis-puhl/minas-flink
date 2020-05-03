package br.ufscar.dc.gsdr.mfog;

import br.ufscar.dc.gsdr.mfog.structs.Cluster;
import br.ufscar.dc.gsdr.mfog.util.Logger;
import br.ufscar.dc.gsdr.mfog.util.MfogManager;
import br.ufscar.dc.gsdr.mfog.util.ServerClient;
import br.ufscar.dc.gsdr.mfog.util.TcpUtil;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.util.Iterator;
import java.util.stream.Stream;

public class TrainingStatic {
    public static void main(String[] args) throws Exception {
        final Logger LOG = Logger.getLogger(TrainingStatic.class);
        String path = "datasets" + File.separator + "models" + File.separator + "offline.csv";
        BufferedReader in = new BufferedReader(new FileReader(path));
        Stream<Cluster> model = in.lines().skip(1).map(Cluster::fromMinasCsv);
        //
        LOG.info("connecting to " + MfogManager.SERVICES_HOSTNAME + ":" + MfogManager.MODEL_STORE_PORT);
        ServerClient<Cluster> socket = new ServerClient<>(Cluster.class, new Cluster(), false, TrainingStatic.class);
        socket.client(MfogManager.SERVICES_HOSTNAME, MfogManager.MODEL_STORE_PORT);
        LOG.info("Sending");
        long sndTime =  System.currentTimeMillis();
        int snd = 0;
        Iterator<Cluster> iterator = model.iterator();
        while (socket.isConnected() && iterator.hasNext()) {
            socket.send(iterator.next());
            if (System.currentTimeMillis() - sndTime > TcpUtil.REPORT_INTERVAL) {
                int speed = ((int) (snd / ((System.currentTimeMillis() - sndTime) * 10e-4)));
                sndTime = System.currentTimeMillis();
                LOG.info("snd=" + snd + " " + speed + " i/s");
            }
        }
        socket.flush();
        socket.close();
        LOG.info("done");
    }
}
