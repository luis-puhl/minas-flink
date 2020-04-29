package br.ufscar.dc.gsdr.mfog;

import br.ufscar.dc.gsdr.mfog.structs.Cluster;
import br.ufscar.dc.gsdr.mfog.util.Logger;
import br.ufscar.dc.gsdr.mfog.util.MfogManager;
import br.ufscar.dc.gsdr.mfog.util.TcpUtil;
import br.ufscar.dc.gsdr.mfog.util.Try;
import org.apache.commons.lang3.SerializationUtils;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

public class ModelStore {
    static List<Cluster> model = new ArrayList<>(100);

    public static void main(String[] args) throws IOException, InterruptedException {
        Logger LOG = Logger.getLogger("ModelStore");
        //
        ServerSocket serverSocket = new ServerSocket(MfogManager.MODEL_STORE_PORT);
        LOG.info("server ready on " + serverSocket.getInetAddress() + ":" + serverSocket.getLocalPort());
        List<Thread> threadPool = new ArrayList<>(3);
        long start = System.currentTimeMillis();
        //
        Socket socket = serverSocket.accept();
        InputStream inputStream = socket.getInputStream();
        LOG.info("Receiving");
        long rcvTime =  System.currentTimeMillis();
        int rcv = 0;
        Thread.sleep(10);
        do {
            try {
                Cluster c = SerializationUtils.deserialize(inputStream);
                model.add(c);
            } catch (Exception e) {
                LOG.error(e);
                break;
            }
            if (System.currentTimeMillis() - rcvTime > TcpUtil.REPORT_INTERVAL) {
                int speed = ((int) (rcv / ((System.currentTimeMillis() - rcvTime) * 10e-4)));
                rcvTime = System.currentTimeMillis();
                LOG.info("rcv=" + rcv + " " + speed + " i/s");
            }
        } while (inputStream.available() > 10);
        socket.close();
        //
        socket = serverSocket.accept();
        OutputStream outputStream = socket.getOutputStream();
        LOG.info("Sending");
        long sndTime =  System.currentTimeMillis();
        int snd = 0;
        Iterator<Cluster> iterator = model.iterator();
        while (socket.isConnected() && iterator.hasNext()) {
            Cluster next = iterator.next();
            try {
                byte[] bytes = SerializationUtils.serialize(next);
                outputStream.write(bytes);
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
        //
        LOG.info("total " + (rcv + snd) + " items in " + (System.currentTimeMillis() - start) * 10e-4 + "s");
        serverSocket.close();
        LOG.info("done");
    }
}
