package br.ufscar.dc.gsdr.mfog;

import br.ufscar.dc.gsdr.mfog.structs.Cluster;
import br.ufscar.dc.gsdr.mfog.structs.Point;
import br.ufscar.dc.gsdr.mfog.util.Logger;
import br.ufscar.dc.gsdr.mfog.util.MfogManager;
import br.ufscar.dc.gsdr.mfog.util.TcpUtil;
import br.ufscar.dc.gsdr.mfog.util.Try;
import org.apache.commons.lang3.SerializationUtils;

import java.io.*;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

public class ModelStore {

    public static void main(String[] args) throws IOException, InterruptedException {
        new ModelStore().detailed();
    }
    
    public void detailed() throws IOException, InterruptedException {
        List<Cluster> model = new ArrayList<>(100);
        Logger LOG = Logger.getLogger("ModelStore");
        //
        ServerSocket serverSocket = new ServerSocket(MfogManager.MODEL_STORE_PORT);
        LOG.info("server ready on " + serverSocket.getInetAddress() + ":" + serverSocket.getLocalPort());
        long start = System.currentTimeMillis();
        //
        Socket socket = serverSocket.accept();
        DataOutputStream writer = new DataOutputStream(new BufferedOutputStream(socket.getOutputStream()));
        DataInputStream reader = new DataInputStream(new BufferedInputStream(socket.getInputStream()));
        LOG.info("Receiving");
        long rcvTime =  System.currentTimeMillis();
        int rcv = 0;
        Thread.sleep(10);
        do {
            try {
                Cluster cluster = Cluster.fromDataInputStream(reader);
                model.add(cluster);
            } catch (Exception e) {
                LOG.error(e);
                break;
            }
            if (System.currentTimeMillis() - rcvTime > TcpUtil.REPORT_INTERVAL) {
                int speed = ((int) (rcv / ((System.currentTimeMillis() - rcvTime) * 10e-4)));
                rcvTime = System.currentTimeMillis();
                LOG.info("rcv=" + rcv + " " + speed + " i/s");
            }
        } while (reader.available() > 10);
        socket.close();
        //
        socket = serverSocket.accept();
        writer = new DataOutputStream(new BufferedOutputStream(socket.getOutputStream()));
        reader = new DataInputStream(new BufferedInputStream(socket.getInputStream()));
        LOG.info("Sending");
        long sndTime =  System.currentTimeMillis();
        int snd = 0;
        Iterator<Cluster> iterator = model.iterator();
        while (socket.isConnected() && iterator.hasNext()) {
            Cluster next = iterator.next();
            next.toDataOutputStream(writer);
            if (System.currentTimeMillis() - sndTime > TcpUtil.REPORT_INTERVAL) {
                int speed = ((int) (snd / ((System.currentTimeMillis() - sndTime) * 10e-4)));
                sndTime = System.currentTimeMillis();
                LOG.info("snd=" + snd + " " + speed + " i/s");
            }
        }
        writer.flush();

        socket.close();
        //
        LOG.info("total " + (rcv + snd) + " items in " + (System.currentTimeMillis() - start) * 10e-4 + "s");
        serverSocket.close();
        LOG.info("done");
    }

    public void og() {
        List<Cluster> model = new ArrayList<>(100);
        // List<String> model = new ArrayList<>(100);
        Logger LOG = Logger.getLogger("ModelStore");
        // TcpUtil<String> server = new TcpUtil<>(
        TcpUtil<Cluster> server = new TcpUtil<>(
                "ModelStore",
                MfogManager.MODEL_STORE_PORT, () -> {
                    LOG.info("current model size =" + model.size());
                    return model.iterator();
                },
                /*
                (s) -> {
                    model.stream().limit(1).forEach((i) -> {
                        if (i.equals(s)) LOG.info("current model size =" + model.size());
                    });
                    return s;
                },
                */
                model
        );
        server.server(2);
        LOG.info("done");
    }
}
