package br.ufscar.dc.gsdr.mfog.util;

import br.ufscar.dc.gsdr.mfog.structs.Point;
import br.ufscar.dc.gsdr.mfog.structs.WithSerializable;

import java.io.*;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.concurrent.*;
import java.util.zip.GZIPInputStream;
import java.util.zip.GZIPOutputStream;

public class ServerClient<T extends WithSerializable<T>> {
    protected final Logger log;
    protected final Class<T> typeParameterClass;
    protected boolean withGzip;

    @Deprecated
    public ServerClient(Class<T> typeParameterClass) throws Exception {
        this(typeParameterClass, false);
    }
    @Deprecated
    public ServerClient(Class<T> typeParameterClass, boolean withGzip) throws Exception {
        this(typeParameterClass, typeParameterClass.getDeclaredConstructor().newInstance(), withGzip);
    }
    @Deprecated
    public ServerClient(Class<T> typeParameterClass, T reusableObject) {
        this(typeParameterClass, reusableObject, false);
    }
    @Deprecated
    public ServerClient(Class<T> typeParameterClass, T reusableObject, boolean withGzip) {
        this(typeParameterClass, reusableObject, withGzip, Object.class);
    }
    public ServerClient(Class<T> typeParameterClass, T reusableObject, boolean withGzip, Class<?> caller) {
        this.reusableObject = reusableObject;
        this.typeParameterClass = typeParameterClass;
        this.withGzip = withGzip;
        this.log = Logger.getLogger(this.getClass(), typeParameterClass, caller);
    }

    public T reusableObject;
    public ServerSocket serverSocket;
    public Socket socket;
    public OutputStream outputStream;
    public InputStream inputStream;
    public DataInputStream reader;
    public DataOutputStream writer;
    public GZIPOutputStream gzipOut = null;
    public long millis;
    public long nano;
    public long i = 0;
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
        log.info("outputStream");
        outputStream = socket.getOutputStream();
        log.info("inputStream");
        inputStream = socket.getInputStream();
        millis = System.currentTimeMillis();
        nano = System.nanoTime();
    }
    public void client(String host, int port) throws IOException, InterruptedException {
        client(host, port, 3, 500);
    }
    public ServerClient<T> client(String host, int port, int maxNumRetries, long delayBetweenRetries) throws IOException, InterruptedException {
        log.info("socket");
        socket = null;
        Exception lastEx = null;
        while (maxNumRetries > 0 || maxNumRetries == -1) {
            try {
                socket = new Socket(host, port);
                log.info("outputStream");
                outputStream = socket.getOutputStream();
                log.info("inputStream");
                inputStream = socket.getInputStream();
                millis = System.currentTimeMillis();
                nano = System.nanoTime();
                return this;
            } catch (Exception e) {
                lastEx = e;
                log.error(e);
                log.error(e.getClass());
                Thread.sleep(delayBetweenRetries);
            }
            if (maxNumRetries != -1) maxNumRetries --;
        }
        throw new IOException(ServerClient.class.getSimpleName() + " could not connect.", lastEx);
    }

    public void send(T toSend) throws IOException {
        if (writer == null) {
            log.info("new writer");
            if (withGzip) {
                gzipOut = new GZIPOutputStream(new BufferedOutputStream(outputStream));
                writer = new DataOutputStream(gzipOut);
            } else {
                writer = new DataOutputStream(new BufferedOutputStream(outputStream));
            }
        }
        toSend.toDataOutputStream(writer);
        i++;
    }
    public void flush() throws IOException {
        log.info("flush");
        writer.flush();
        if (gzipOut != null) {
            gzipOut.finish();
            gzipOut.flush();
        }
        outputStream.flush();
    }
    public boolean isConnected() {
        return socket != null && socket.isConnected();
    }

    public T receive(T reusableObject) throws IOException {
        if (reader == null) {
            if (withGzip) {
                reader = new DataInputStream(new GZIPInputStream(new BufferedInputStream(inputStream)));
            } else {
                reader = new DataInputStream(new BufferedInputStream(inputStream));
            }
        }
        T t = reusableObject.reuseFromDataInputStream(reader);
        i++;
        return t;
    }
    protected boolean hasNext = true;
    public boolean hasNext() {
        return hasNext && socket.isConnected();
    }
    public T next() throws IOException {
        try {
            reusableObject = receive(reusableObject);
            return reusableObject;
        } catch (java.io.EOFException e) {
            hasNext = false;
            return null;
        }
    }

    public void closeSocket() throws IOException {
        log.debug("socket.close()");
        if (socket != null && !socket.isClosed()) {
            socket.close();
        }
        socket = null;
        long millisDiff = System.currentTimeMillis() - millis;
        long nanoDiff = System.nanoTime() - nano;
        long speed = ((i *10^4)/ millisDiff);
        log.info(i + " items, " + millisDiff + " ms, " + nanoDiff + " ns, " + speed + " i/s");
    }
    public void closeServer() throws IOException {
        if (serverSocket != null && !serverSocket.isClosed()) {
            serverSocket.close();
        }
        serverSocket = null;
    }

    public static void main(String[] args) throws Exception {
        Logger log = Logger.getLogger(ServerClient.class);
        boolean isServer = args.length > 0;
        int port = 9999;
        String kind = "client";
        if (isServer) {
            kind = "server";
        }
        log.info("Self test >" + kind);

        log.info("Point List full ");
        ServerClient<Point> serverClient = new ServerClient<>(Point.class);
        //
        if (isServer) {
            serverClient.server(port);
            serverClient.serverAccept();
        } else {
            serverClient.client("localhost", port);
        }
        int i = 0;
        if (isServer) {
            Point zero = Point.zero(22);
            for (; i < 653457; i++) {
                zero.id = i;
                serverClient.send(zero);
            }
            serverClient.flush();
        } else {
            while (serverClient.hasNext()){
                serverClient.next();
                i++;
            }
        }
        serverClient.closeSocket();
        serverClient.closeServer();
    }
}