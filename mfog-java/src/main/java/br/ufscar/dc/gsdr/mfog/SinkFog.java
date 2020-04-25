package br.ufscar.dc.gsdr.mfog;

import br.ufscar.dc.gsdr.mfog.structs.LabeledExample;
import br.ufscar.dc.gsdr.mfog.util.Logger;
import br.ufscar.dc.gsdr.mfog.util.MfogManager;
import br.ufscar.dc.gsdr.mfog.util.Try;

import java.io.*;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Queue;
import java.util.concurrent.ConcurrentLinkedQueue;

public class SinkFog {
    static Logger LOG = Logger.getLogger("SinkFog");
    static Queue<LabeledExample> fromClassifier = new ConcurrentLinkedQueue<>();
    static Queue<LabeledExample> fromSource = new ConcurrentLinkedQueue<>();

    public static void main(String[] args) throws IOException, InterruptedException {
        ServerSocket serverSocket = new ServerSocket(MfogManager.SINK_MODULE_TEST_PORT);
        LOG.info("server ready on " + serverSocket.getInetAddress() + ":" + serverSocket.getLocalPort());
        /*Thread threadFromClassifier = new Thread(() -> {
            class Rcv implements Runnable{
                int i = 0;
                boolean done = false;
                Socket socket;
                Thread thread;
                @Override
                public void run() {
                    i += handleClassifier(socket, fromClassifier);
                    done = true;
                }
            }
            List<Rcv> threadPool = new ArrayList<>(10);
            int i = 0;
            while (i < 100 || fromSource.size() > 0 || fromClassifier.size() > 0) {
                try {
                    Rcv rcv = new Rcv();
                    rcv.socket = serverSocket.accept();
                    rcv.thread = new Thread(rcv);
                    rcv.thread.start();
                    threadPool.add(rcv);
                } catch (IOException e) {
                    LOG.info(e.getMessage());
                }
                for (Rcv rcv : threadPool) {
                    if (rcv.done) {
                        try {
                            rcv.thread.join();
                        } catch (InterruptedException e) {
                            LOG.info(e.getMessage());
                        }
                        i += rcv.i;
                        threadPool.remove(rcv);
                    }
                }
            }
            for (Rcv rcv : threadPool) {
                try {
                    rcv.thread.join();
                } catch (InterruptedException e) {
                    LOG.info(e.getMessage());
                }
            }
        });*/
        Socket socket = serverSocket.accept();
        Thread threadFromClassifier = new Thread(() -> handleClassifier("classifier", socket, fromClassifier));
        threadFromClassifier.start();
        //
        LOG.info("connecting to " + MfogManager.SERVICES_HOSTNAME + ":" + MfogManager.SOURCE_EVALUATE_DATA_PORT);
        Socket socketSource = new Socket(InetAddress.getByName(MfogManager.SERVICES_HOSTNAME), MfogManager.SOURCE_EVALUATE_DATA_PORT);
        Thread threadFromSource = new Thread(() -> handleClassifier("source", socketSource, fromSource));
        threadFromSource.start();
        //
        long i = 0;
        long matches = 0;
        while (i < 100 || fromSource.size() > 0 || fromClassifier.size() > 0) {
            LabeledExample peek = fromClassifier.peek();
            if (peek == null) continue;
            for (LabeledExample source : fromSource) {
                if (peek.point.id == source.point.id) {
                    if (peek.label.equals(source.label)) {
                        matches++;
                    }
                    i++;
                    fromClassifier.poll();
                    break;
                }
            }
            Thread.sleep(10);
        }
        LOG.info("i=" + i + " matches=" + matches + " misses=" + (i - matches));
        //
        try {
            threadFromClassifier.join();
            threadFromSource.join();
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
        serverSocket.close();
    }

    static public int handleClassifier(String name, Socket socket, Queue<LabeledExample> queue) {
        LOG.info("connected " + name);
        Try.apply(socket::shutdownOutput);
        long startReceive = System.currentTimeMillis();
        InputStream inputStream;
        if ((inputStream = Try.apply(socket::getInputStream).get) == null) {
            Try.apply(socket::close);
            return 0;
        }
        Try.apply(() -> Thread.sleep(1000));
        int rec = 0;
        while (queue.size() < 1000 || Try.apply(inputStream::available).get > 0) {
            Iterator<String> iterator = new BufferedReader(new InputStreamReader(inputStream)).lines().iterator();
            while (iterator.hasNext()) {
                String next = iterator.next();
                try {
                    LabeledExample labeledExample = LabeledExample.fromJson(next);
                    queue.add(labeledExample);
                    rec++;
                } catch (Exception e) {
                    LOG.info(next + " => " + e.getMessage());
                }
                Try.apply(() -> Thread.sleep(10));
            }
        }
        Try.apply(socket::shutdownInput);
        Try.apply(socket::close);
        LOG.info(name + " received " + rec + " items in " + (System.currentTimeMillis() - startReceive) * 10e-4 + "s");
        return rec;
    }
}
