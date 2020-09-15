package br.ufscar.dc.gsdr.mfog.util;

import br.ufscar.dc.gsdr.mfog.structs.SelfDataStreamSerializable;
import br.ufscar.dc.gsdr.mfog.structs.Point;
import com.esotericsoftware.kryo.io.Output;

import java.io.*;
import java.net.ConnectException;
import java.net.InetSocketAddress;
import java.net.ServerSocket;
import java.net.Socket;

@Deprecated
public class Tcp<T extends SelfDataStreamSerializable<T>> implements Closeable {
    protected final Logger log;
    protected final Class<T> typeParameterClass;
    // protected final Kryo kryo;
    public T reusableObject;
    public ServerSocket serverSocket;
    public Socket socket;
    public OutputStream outputStream;
    public InputStream inputStream;
    public DataInputStream reader;
    public DataOutputStream writer;
    // public GZIPOutputStream gzipOut = null;
    public long millis;
    public long nano;
    public long i = 0;
    // public boolean withGzip = MfogManager.USE_GZIP;
    protected boolean hasNext;
    protected T next;

    @SuppressWarnings("unchecked")
    public Tcp(Class<T> typeParameterClass, T reusableObject, Class<?> caller) {
        this.reusableObject = reusableObject;
        this.typeParameterClass = typeParameterClass;
        this.log = Logger.getLogger(this.getClass(), typeParameterClass, caller);
        // this.kryo = Serializers.getKryo();
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
        Logger log = Logger.getLogger(Tcp.class);
        int port = 8888;
        String kind = "client";
        if (isServer) {
            kind = "server";
        }
        log.info("Self test >" + kind);

        log.info("Point List full ");
        Tcp<Point> tcp = new Tcp<>(Point.class, new Point(), Tcp.class);
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
        log.info("server ready on " + serverSocket.getInetAddress() + ":" + serverSocket.getLocalPort());
    }

    public void serverAccept() throws IOException {
        if (socket != null && !socket.isClosed()) {
            socket.close();
        }
        log.info("accept");
        socket = serverSocket.accept();
        outputStream = socket.getOutputStream();
        inputStream = socket.getInputStream();
        hasNext = true;
        millis = System.currentTimeMillis();
        nano = System.nanoTime();
    }

    public void client(String host, int port) throws IOException, InterruptedException {
        client(host, port, 3, 1000, 0);
    }

    public Tcp<T> client(String host, int port, long maxNumRetries, long delayBetweenRetries, int connectionTimeout) throws IOException, InterruptedException {
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
        throw new IOException(Tcp.class.getSimpleName() + " could not connect.", lastEx);
    }

    public boolean send(T toSend) throws IOException {
        if (writer == null) {
            log.info("new writer");
            // if (withGzip) {
            //     gzipOut = new GZIPOutputStream(new BufferedOutputStream(outputStream));
            //     writer = new DataOutputStream(gzipOut);
            // } else {
            writer = new DataOutputStream(new BufferedOutputStream(outputStream));
            // }
        }
        // toSend.toDataOutputStream(writer);
        // serializer.write(kryo, writer, toSend);
        toSend.write(writer, toSend);
        i++;
        return true;
    }

    public void flush() throws IOException {
        log.info("flush");
        writer.flush();
        // if (gzipOut != null) {
        //     gzipOut.finish();
        //     gzipOut.flush();
        // }
        outputStream.flush();
    }

    public boolean isConnected() {
        return socket != null && socket.isConnected();
    }

    public boolean hasNext() throws IOException {
        if (this.next != null) {
            return true;
        }
        if (!(hasNext && socket != null && socket.isConnected())) {
            hasNext = false;
            return false;
        }
        if (reader == null) {
            // if (withGzip) {
            //     reader = new DataInputStream(new GZIPInputStream(new BufferedInputStream(inputStream)));
            // } else {
            reader = new DataInputStream(new BufferedInputStream(inputStream));
            // }
        }
        // next = serializer.read(kryo, reader, typeParameterClass);
        try {
            next = reusableObject.read(reader, reusableObject);
            i++;
            return true;
        } catch (EOFException e) {
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