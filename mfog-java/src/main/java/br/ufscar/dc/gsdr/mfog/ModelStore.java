package br.ufscar.dc.gsdr.mfog;

import java.io.*;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.ArrayList;
import java.util.List;
import java.util.logging.Logger;

public class ModelStore {
    static Logger LOG = Logger.getLogger(ModelStoreAkka.class.getName());
    static List<String> model = new ArrayList<>(100);

    public static void main(String[] args) throws InterruptedException {
        Thread senderTread = new Thread(ModelStore::sender);
        senderTread.start();

        Thread receiverTread = new Thread(ModelStore::receiver);
        receiverTread.start();

        senderTread.join();
        receiverTread.join();
    }

    static void sender() {
        ServerSocket senderServer;
        if ((senderServer = Try.apply(() -> new ServerSocket(9997)).get) == null) return;
        //
        final Logger LOG = Logger.getLogger(ModelStoreAkka.class.getName());
        LOG.info("Sender ready");
        for (int i = 0; i < 3; i++) {
            Socket socket;
            if ((socket = Try.apply(senderServer::accept).get) == null) break;
            long start = System.currentTimeMillis();
            LOG.info("sender connected");
            OutputStream outputStream;
            if ((outputStream = Try.apply(socket::getOutputStream).get) == null) {
                Try.apply(socket::close);
                break;
            }
            PrintStream out = new PrintStream(outputStream);
            LOG.info("sending " + model.get(1));
            model.forEach(out::println);
            out.flush();
            Try.apply(socket::close);
            LOG.info("sent " + model.size() + " items in " + (System.currentTimeMillis() - start) * 10e-4 + "s");
        }
        Try.apply(senderServer::close);
  }

    static void receiver() {
        ServerSocket receiverServer;
        if ((receiverServer = Try.apply(() -> new ServerSocket(9998)).get) == null) return;
        LOG.info("Receiver ready");
        for (int i = 0; i < 3; i++) {
            Socket socket;
            if ((socket = Try.apply(receiverServer::accept).get) == null) break;
            LOG.info("Receiver connected\nappending to " + model.size());
            InputStream inputStream;
            if ((inputStream = Try.apply(socket::getInputStream).get) == null) {
                Try.apply(socket::close);
                break;
            }
            BufferedReader read = new BufferedReader(new InputStreamReader(inputStream));
            read.lines().forEach(model::add);
            Try.apply(socket::close);
            LOG.info("total " + model.size());
        }
        Try.apply(receiverServer::close);
  }

}
