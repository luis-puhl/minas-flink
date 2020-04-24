package br.ufscar.dc.gsdr.mfog.util;

import java.io.*;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Iterator;
import java.util.List;
import java.util.function.Function;
import java.util.stream.Stream;

public class TcpUtil<T> {
    public interface ToSendStream<T> {
        Stream<T> get() throws Exception;
    }

    ToSendStream<T> toSend;
    Function<String, T> toReceive;
    Collection<T> received;
    int port;
    String serviceName;
    Logger LOG;
    public long waitBeforeRcv = 10;
    public long waitBetweenRcv = 10;

    public TcpUtil(String serviceName, int port, ToSendStream<T> toSend, Function<String, T> toReceive, Collection<T> received) {
        this.serviceName = serviceName;
        LOG = Logger.getLogger(serviceName);
        this.port = port;
        this.toSend     = toSend    == null ? Stream::empty : toSend;
        this.toReceive  = toReceive == null ? (s) -> null : toReceive;
        this.received   = toReceive == null ? new ArrayList<>(100) : received;
    }

    public int client() throws IOException {
        LOG.info("connecting to " + MfogManager.SERVICES_HOSTNAME + ":" + port);
        Socket socket = new Socket(InetAddress.getByName(MfogManager.SERVICES_HOSTNAME), port);
        return communicate(socket);
    }

    int communicate(Socket socket) {
        long start = System.currentTimeMillis();
        LOG.info("connected");
        //
        OutputStream outputStream;
        if ((outputStream = Try.apply(socket::getOutputStream).get) == null) {
            Try.apply(socket::close);
            return -1;
        }
        PrintStream out = new PrintStream(outputStream);
        //
        Stream<T> toSendStream;
        if ((toSendStream = Try.apply(toSend::get).get) == null) {
            Try.apply(socket::close);
            return -1;
        }
        Iterator<T> iterator = toSendStream.iterator();
        int sent = 0;
        while (iterator.hasNext()) {
            T next = iterator.next();
            out.println(next);
            out.flush();
            sent++;
        }
        out.flush();
        final int sentFinal = sent;
        LOG.info("sent " + sentFinal + " items in " + (System.currentTimeMillis() - start) * 10e-4 + "s");
        Try.apply(socket::shutdownOutput);
        //
        long startReceive = System.currentTimeMillis();
        InputStream inputStream;
        if ((inputStream = Try.apply(socket::getInputStream).get) == null) {
            Try.apply(socket::close);
            return -1;
        }
        Try.apply(() -> Thread.sleep(waitBeforeRcv));
        int rec = 0;
        if (Try.apply(inputStream::available).get > 0) {
            BufferedReader read = new BufferedReader(new InputStreamReader(inputStream));
            iterator = read.lines().map(toReceive).iterator();
            while (iterator.hasNext()) {
                T next = iterator.next();
                received.add(next);
                rec++;
                Try.apply(() -> Thread.sleep(waitBetweenRcv));
            }
            Try.apply(socket::shutdownInput);
        }
        //
        Try.apply(socket::close);
        final int recFinal = rec;
        LOG.info( "received " + recFinal + " items in " + (System.currentTimeMillis() - startReceive) * 10e-4 + "s");
        final int total = recFinal + sentFinal;
        LOG.info("total " + total + " items in " + (System.currentTimeMillis() - start) * 10e-4 + "s");
        return rec + sent;
    }

    public void server() {
        ServerSocket serverSocket;
        if ((serverSocket = Try.apply(() -> new ServerSocket(port)).get) == null) return;
        //
        LOG.info("server ready on " + serverSocket.getInetAddress() + ":" + serverSocket.getLocalPort());
        List<Thread> threadPool = new ArrayList<>(3);
        for (int i = 0; i < 10; i++) {
            Socket socket;
            if ((socket = Try.apply(serverSocket::accept).get) == null) break;
            Thread commThread = new Thread(() -> communicate(socket));
            threadPool.add(commThread);
            commThread.start();
        }
        for (Thread thread : threadPool) {
            try {
                thread.join();
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }
        Try.apply(serverSocket::close);
    }
}
