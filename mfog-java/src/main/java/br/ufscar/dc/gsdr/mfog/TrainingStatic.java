package br.ufscar.dc.gsdr.mfog;

import br.ufscar.dc.gsdr.mfog.structs.Cluster;
import br.ufscar.dc.gsdr.mfog.util.Logger;
import br.ufscar.dc.gsdr.mfog.util.MfogManager;
import br.ufscar.dc.gsdr.mfog.util.TcpUtil;
import org.apache.commons.lang3.SerializationUtils;

import java.io.*;
import java.net.InetAddress;
import java.net.Socket;
import java.util.Iterator;
import java.util.stream.Stream;

public class TrainingStatic {
    public static void main(String[] args) throws IOException {
        new TrainingStatic().ns();
    }
    public void ns()  throws IOException {
        final Logger LOG = Logger.getLogger("TrainingStatic");
        String path = "datasets" + File.separator + "models" + File.separator + "offline.csv";
        BufferedReader in = new BufferedReader(new FileReader(path));
        Stream<Cluster> model = in.lines().skip(1).map(Cluster::fromMinasCsv);
        //
        LOG.info("connecting to " + MfogManager.SERVICES_HOSTNAME + ":" + MfogManager.MODEL_STORE_PORT);
        Socket socket = new Socket(InetAddress.getByName(MfogManager.SERVICES_HOSTNAME), MfogManager.MODEL_STORE_PORT);
        DataOutputStream writer = new DataOutputStream(new BufferedOutputStream(socket.getOutputStream()));
        DataInputStream reader = new DataInputStream(new BufferedInputStream(socket.getInputStream()));
        LOG.info("Sending");
        long sndTime =  System.currentTimeMillis();
        int snd = 0;
        Iterator<Cluster> iterator = model.iterator();
        while (socket.isConnected() && iterator.hasNext()) {
            Cluster next = iterator.next();
            try {
                next.toDataOutputStream(writer);
            } catch (Exception e) {
                LOG.error(e);
                break;
            }
            if (System.currentTimeMillis() - sndTime > TcpUtil.REPORT_INTERVAL) {
                int speed = ((int) (snd / ((System.currentTimeMillis() - sndTime) * 10e-4)));
                sndTime = System.currentTimeMillis();
                LOG.info("snd=" + snd + " " + speed + " i/s");
            }
        }
        socket.close();
        LOG.info("done");
    }
    public void og() throws IOException {
        final Logger LOG = Logger.getLogger("TrainingStatic");
        String path = "datasets" + File.separator + "models" + File.separator + "offline.csv";
        BufferedReader in = new BufferedReader(new FileReader(path));
        Stream<String> model = in.lines().skip(1).map(Cluster::fromMinasCsv).map(c -> c.json().toString());
        //
        TcpUtil<String> tcp = new TcpUtil<>("TrainingStatic", MfogManager.MODEL_STORE_PORT, model::iterator, null);
        tcp.client();
        LOG.info("done");
    }
}
