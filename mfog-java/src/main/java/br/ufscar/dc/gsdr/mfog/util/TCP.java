package br.ufscar.dc.gsdr.mfog.util;

import br.ufscar.dc.gsdr.mfog.structs.Point;
import br.ufscar.dc.gsdr.mfog.structs.Serializers;
import com.esotericsoftware.kryo.Kryo;
import com.esotericsoftware.kryo.Serializer;
import com.esotericsoftware.kryo.io.Input;
import com.esotericsoftware.kryo.io.Output;

import java.io.*;
import java.net.ConnectException;
import java.net.InetSocketAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.zip.GZIPInputStream;
import java.util.zip.GZIPOutputStream;

public class TCP<T> implements Closeable {
    protected final Logger log;
    protected final Class<T> typeParameterClass;
    protected final Kryo kryo;
    public T reusableObject;
    public ServerSocket serverSocket;
    public Socket socket;
    public OutputStream outputStream;
    public InputStream inputStream;
    public Input reader;
    public Output writer;
    public GZIPOutputStream gzipOut = null;
    public long millis;
    public long nano;
    public long i = 0;
    public boolean withGzip = MfogManager.USE_GZIP;
    protected boolean hasNext;
    protected Serializer<T> serializer;
    T next;

    @SuppressWarnings("unchecked")
    public TCP(Class<T> typeParameterClass, T reusableObject, Class<?> caller) {
        this.reusableObject = reusableObject;
        this.typeParameterClass = typeParameterClass;
        this.log = Logger.getLogger(this.getClass(), typeParameterClass, caller);
        this.kryo = Serializers.getKryo();
        try {
            serializer = this.kryo.getRegistration(typeParameterClass).getSerializer();
        } catch (IllegalArgumentException e) {
            throw new IllegalArgumentException("No serializer for class " + typeParameterClass, e);
        }
    }

    public static void main(String[] args) throws Exception {
        new Thread(() -> {
            try {
                selfTest(true);
            } catch (Exception e) {
                e.printStackTrace();
            }
        }).start();
        new Thread(() -> {
            try {
                selfTest(false);
            } catch (Exception e) {
                e.printStackTrace();
            }
        }).start();
    }
    public static void selfTest(boolean isServer) throws Exception {
        Logger log = Logger.getLogger(TCP.class);
        int port = 8888;
        String kind = "client";
        if (isServer) {
            kind = "server";
        }
        log.info("Self test >" + kind);

        log.info("Point List full ");
        TCP<Point> tcp = new TCP<>(Point.class, new Point(), TCP.class);
        //
        if (isServer) {
            tcp.server(port);
            tcp.serverAccept();
        } else {
            tcp.client("localhost", port);
        }
        int i = 0;
        if (isServer) {
            Point zero = Point.zero(22);
            for (; i < 653457; i++) {
                zero.id = i;
                tcp.send(zero);
            }
            tcp.flush();
        } else {
            while (tcp.hasNext()) {
                tcp.next();
                i++;
            }
        }
        tcp.close();
        tcp.closeServer();
    }

    public void server(int port) throws IOException {
        if (serverSocket != null && !serverSocket.isClosed()) {
            serverSocket.close();
        }
        serverSocket = new ServerSocket(port);
    }

    public void serverAccept() throws IOException {
        if (socket != null && !socket.isClosed()) {
            socket.close();
        }
        log.info("accept");
        socket = serverSocket.accept();
        outputStream = socket.getOutputStream();
        inputStream = socket.getInputStream();
        millis = System.currentTimeMillis();
        nano = System.nanoTime();
    }

    public void client(String host, int port) throws IOException, InterruptedException {
        client(host, port, 3, 1000, 0);
    }

    public TCP<T> client(String host, int port, long maxNumRetries, long delayBetweenRetries, int connectionTimeout) throws IOException, InterruptedException {
        log.info("socket");
        socket = null;
        Exception lastEx = null;
        while (maxNumRetries > 0 || maxNumRetries == -1) {
            try {
                InetSocketAddress inetSocketAddress = new InetSocketAddress(host, port);
                socket = new Socket();
                socket.connect(inetSocketAddress, connectionTimeout);
                outputStream = socket.getOutputStream();
                inputStream = socket.getInputStream();
                hasNext = true;
                millis = System.currentTimeMillis();
                nano = System.nanoTime();
                return this;
            } catch (ConnectException e) {
                lastEx = e;
                log.warn(e.getMessage());
                Thread.sleep(delayBetweenRetries);
            }
            if (maxNumRetries != -1) maxNumRetries--;
        }
        throw new IOException(TCP.class.getSimpleName() + " could not connect.", lastEx);
    }

    // FileOutputStream snd;
    public boolean send(T toSend) throws IOException {
        if (writer == null) {
            log.info("new writer");
            if (withGzip) {
                gzipOut = new GZIPOutputStream(new BufferedOutputStream(outputStream));
                writer = new Output(gzipOut);
            } else {
                writer = new Output(new BufferedOutputStream(outputStream));
            }
            // snd = new FileOutputStream("snd.bin");
        }
        try {
            serializer.write(kryo, writer, toSend);
            // snd.write(writer.getBuffer());
            i++;
            return true;
        } catch (Exception e) {
            log.warn(e.getMessage());
            throw e;
            // return false;
        }
    }

    public void flush() throws IOException {
        log.info("flush");
        if (writer != null) {
            try {
                writer.flush();
            } catch (Exception e) {
                log.warn(e.getMessage());
            }
        }
        if (gzipOut != null) {
            gzipOut.finish();
            gzipOut.flush();
        }
        outputStream.flush();
    }

    public boolean isConnected() {
        return socket != null && socket.isConnected();
    }

    // FileOutputStream rcv;
    public boolean hasNext() {
        if (!hasNext || !socket.isConnected()) {
            hasNext = false;
            return false;
        }
        if (reader == null) {
            if (withGzip) {
                try {
                    reader = new Input(new GZIPInputStream(new BufferedInputStream(inputStream)));
                } catch (IOException e) {
                    e.printStackTrace();
                    System.exit(20);
                }
            } else {
                reader = new Input(new BufferedInputStream(inputStream));
            }
            // rcv = new FileOutputStream("rcv.bin");
        }
        // System.out.println(reader.getBuffer().length);
        // rcv.write(reader.getBuffer());
        try {
            next = serializer.read(kryo, reader, typeParameterClass);
            i++;
            return true;
        } catch (Exception e) {
            hasNext = false;
            next = null;
        }
        return false;
    }

    public T next() throws EOFException {
        if (this.next == null) {
            throw new NullPointerException("No next available.");
        }
        T next = this.next;
        this.next = null;
        return next;
    }

    public void close() throws IOException {
        log.debug("socket.close()");
        if (socket != null && !socket.isClosed()) {
            socket.close();
        }
        socket = null;
        long millisDiff = System.currentTimeMillis() - millis;
        long nanoDiff = System.nanoTime() - nano;
        long speed = millisDiff == 0 ? 0 : ((i * 10 ^ 4) / millisDiff);
        log.info(i + " items, " + millisDiff + " ms, " + nanoDiff + " ns, " + speed + " i/s");
    }

    public void closeServer() throws IOException {
        log.debug("serverSocket.close()");
        if (serverSocket != null && !serverSocket.isClosed()) {
            serverSocket.close();
        }
        serverSocket = null;
    }
}