package br.ufscar.dc.gsdr.mfog.util;

import br.ufscar.dc.gsdr.mfog.structs.Point;
import org.apache.commons.lang3.SerializationUtils;

import java.io.*;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.nio.ByteBuffer;
import java.util.*;

public class TcpUtil<T extends Serializable> {

    public static final int REPORT_INTERVAL = 5000;

    public interface ToSend<T> {
        Iterator<T> get() throws Exception;
    }
    public class Sender implements Runnable {
        int sent = 0;
        Socket socket;
        Sender(Socket socket) {
            this.socket = socket;
        }

        @Override
        public void run() {
            try {
                long start = System.currentTimeMillis();
                DataOutputStream writer = new DataOutputStream(new BufferedOutputStream(socket.getOutputStream()));
                BufferedInputStream reader = new BufferedInputStream(socket.getInputStream());
                Iterator<T> iterator = toSend.get();
                long sentTime = System.currentTimeMillis();
                while (iterator.hasNext() && socket.isConnected()) {
                    final T next = iterator.next();
                    byte[] message = SerializationUtils.serialize(next);
                    writer.write(ByteBuffer.allocate(4).putInt(message.length).array());
                    // writer.writeInt(message.length);
                    writer.write(message);
                    sent++;
                    if (sent % 10 ==0 && socket.isConnected()) {
                        writer.flush();
                    }
                    if (System.currentTimeMillis() - sentTime > REPORT_INTERVAL) {
                        int speed = ((int) (sent / ((System.currentTimeMillis() - sentTime) * 10e-4)));
                        sentTime = System.currentTimeMillis();
                        LOG.info("sent=" + sent + " " + speed + " i/s");
                    }
                }
                if (socket.isConnected()) {
                    writer.flush();
                }
                // reader.read();
                LOG.info("sent " + sent + " items in " + (System.currentTimeMillis() - start) * 10e-4 + "s");
            } catch (Exception e) {
                LOG.error(e);
            }
        }
    }
    public class Receiver implements Runnable {
        int rec = 0;
        Socket socket;
        Receiver(Socket socket) {
            this.socket = socket;
        }
        @Override
        public void run() {
            if (received == null) {
                LOG.info("no buffer to receive");
                return;
            }
            try {
                long start = System.currentTimeMillis();
                DataInputStream reader = new DataInputStream(new BufferedInputStream(socket.getInputStream()));
                long sentTime = System.currentTimeMillis();
                while (socket.isConnected()) {
                    int length;
                    try {
                        length = reader.readInt();
                    } catch (Exception e) {
                        break;
                    }
                    if (length == 0) continue;
                    byte[] message = new byte[length];
                    reader.readFully(message);
                    // LOG.info("rcv " + length);
                    try {
                        T next = SerializationUtils.deserialize(message);
                        received.add(next);
                        rec++;
                    } catch (Exception e) {
                        LOG.error("rcv " + length + " => " + Arrays.toString(message));
                    }
                    if (System.currentTimeMillis() - sentTime > REPORT_INTERVAL) {
                        int speed = ((int) (rec / ((System.currentTimeMillis() - sentTime) * 10e-4)));
                        sentTime = System.currentTimeMillis();
                        LOG.info("rec=" + rec + " " + speed + " i/s");
                    }
                }
                LOG.info("rec " + rec + " items in " + (System.currentTimeMillis() - start) * 10e-4 + "s");
            } catch (Exception e) {
                LOG.error(e);
            }
        }
    }

    ToSend<T> toSend;
    Collection<T> received;
    int port;
    String serviceName;
    Logger LOG;

    public TcpUtil(String serviceName, int port, ToSend<T> toSend, Collection<T> received) {
        this.serviceName = serviceName;
        LOG = Logger.getLogger(serviceName);
        this.port = port;
        this.toSend     = toSend    == null ? Collections::emptyIterator : toSend;
        this.received   = received;
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
        Sender sender = new Sender(socket);
        Thread senderThread = new Thread(sender);
        Receiver receiver = new Receiver(socket);
        Thread receiverThread = new Thread(receiver);
        //
        senderThread.start();
        receiverThread.start();
        //
        try {
            senderThread.join();
            receiverThread.join();
            socket.close();
        } catch (InterruptedException | IOException e) {
            LOG.error(e);
        }
        final int total = sender.sent + receiver.rec;
        LOG.info("total " + total + " items in " + (System.currentTimeMillis() - start) * 10e-4 + "s");
        return total;
    }

    public void server() {
        server(1);
    }
    public void server(int services) {
        try (ServerSocket serverSocket = new ServerSocket(port)) {
            LOG.info("server ready on " + serverSocket.getInetAddress() + ":" + serverSocket.getLocalPort());
            List<Thread> threadPool = new ArrayList<>(services);
            for (int i = 0; i < services; i++) {
                try (Socket socket = serverSocket.accept()){
                    LOG.info("accept");
                    Thread commThread = new Thread(() -> communicate(socket));
                    threadPool.add(commThread);
                    commThread.start();
                } catch (IOException e) {
                    LOG.error(e);
                    break;
                }
            }
            for (Thread thread : threadPool) {
                try {
                    thread.join();
                } catch (InterruptedException e) {
                    LOG.error(e);
                }
            }
        } catch (IOException e) {
            LOG.error(e);
        }
    }

    public static void main(String[] args) {
        TcpUtil<Point> testService = new TcpUtil<>("test service", 8888, null, null);
        testService.testServer();
    }
    void testServer() {
        Point point = Point.zero(22);
        Thread server = new Thread(() -> {
            try {
                ServerSocket serverSocket = new ServerSocket(port);
                LOG.info("ServerSocket ready");
                Socket socket = serverSocket.accept();
                LOG.info("ServerSocket accepted");
                DataOutputStream writer = new DataOutputStream(new BufferedOutputStream(socket.getOutputStream()));
                DataInputStream reader = new DataInputStream(new BufferedInputStream(socket.getInputStream()));
                byte[] message = point.toBytes();
                writer.writeInt(message.length);
                writer.write(message);
                writer.flush();
                LOG.info("written " + point);
                socket.close();
                serverSocket.close();
            } catch (IOException e) {
                e.printStackTrace();
            }
        });
        server.start();
        //
        Point rcv;
        try {
            Socket socket = new Socket(InetAddress.getByName("localhost"), port);
            LOG.info("Socket connected");
            DataOutputStream writer = new DataOutputStream(new BufferedOutputStream(socket.getOutputStream()));
            DataInputStream reader = new DataInputStream(new BufferedInputStream(socket.getInputStream()));
            int length = reader.readInt();
            if (length > 0) {
                byte[] message = new byte[length];
                reader.readFully(message, 0, message.length);
                rcv = Point.fromBytes(message);
                LOG.info("received " + rcv);
                if (!point.equals(rcv)) {
                    throw new AssertionError("Fuck you.");
                }
            }
            socket.close();
        } catch (IOException e) {
            e.printStackTrace();
        }
        try {
            server.join();
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }
}
