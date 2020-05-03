package br.ufscar.dc.gsdr.mfog.util;

import br.ufscar.dc.gsdr.mfog.structs.Point;
import br.ufscar.dc.gsdr.mfog.structs.WithSerializable;

import java.io.*;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.zip.GZIPInputStream;
import java.util.zip.GZIPOutputStream;

public class ServerClient<T extends WithSerializable<T>> {
    protected Logger log;
    protected final Class<T> typeParameterClass;
    protected T reusableObject;
    protected int port;
    protected boolean withGzip;
    ServerClient(Class<T> typeParameterClass, int port) throws Exception {
        this(typeParameterClass, port, false);
    }
    ServerClient(Class<T> typeParameterClass, int port, boolean withGzip) throws Exception {
        this.reusableObject = typeParameterClass.getDeclaredConstructor().newInstance();
        this.typeParameterClass = typeParameterClass;
        this.log = Logger.getLogger(this.getClass(), typeParameterClass);
        this.port = port;
        this.withGzip = withGzip;
    }

    ServerSocket serverSocket;
    Socket socket;
    OutputStream outputStream;
    InputStream inputStream;
    DataInputStream reader;
    DataOutputStream writer;
    GZIPOutputStream gzipOut = null;
    public void server() throws IOException {
        if (serverSocket != null && !serverSocket.isClosed()) {
            serverSocket.close();
        }
        serverSocket = new ServerSocket(port, 0, InetAddress.getByName("localhost"));
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
    }
    public void client() throws IOException, InterruptedException {
        log.info("socket");
        do {
            try {
                socket = new Socket("localhost", port);
            } catch (Exception e) {
                log.error(e);
                log.error(e.getClass());
                log.error("Connection failed, will retry in 1sec");
                Thread.sleep(1000);
            }
        } while (socket == null);
        log.info("outputStream");
        outputStream = socket.getOutputStream();
        log.info("inputStream");
        inputStream = socket.getInputStream();
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

    public T receive(T reusableObject) throws IOException {
        if (reader == null) {
            if (withGzip) {
                reader = new DataInputStream(new GZIPInputStream(new BufferedInputStream(inputStream)));
            } else {
                reader = new DataInputStream(new BufferedInputStream(inputStream));
            }
        }
        return reusableObject.reuseFromDataInputStream(reader);
    }
    protected boolean hasNext = true;
    public boolean hasNext() {
        return hasNext && socket.isConnected() && !socket.isInputShutdown();
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

    public void close() throws IOException {
        log.info("socket.close()");
        if (socket != null && !socket.isClosed()) {
            socket.close();
        }
        if (serverSocket != null && !serverSocket.isClosed()) {
            serverSocket.close();
        }
        socket = null;
        serverSocket = null;
    }

    public static void main(String[] args) throws Exception {
        Logger log = Logger.getLogger(ServerClient.class);
        boolean isServer = args.length > 0;
        String kind = "client";
        if (isServer) {
            kind = "server";
        }
        log.info("Self test >" + kind);

        log.info("Point List full ");
        ServerClient<Point> serverClient = new ServerClient<>(Point.class, 9999);
        //
        if (isServer) {
            serverClient.server();
            serverClient.serverAccept();
        } else {
            serverClient.client();
        }
        int i = 0;
        long millis = System.currentTimeMillis();
        long nano = System.nanoTime();
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
        serverClient.close();
        long millisDiff = System.currentTimeMillis() - millis;
        long nanoDiff = System.nanoTime() - nano;
        log.info(kind + " millisDiff=" + millisDiff + " nanoDiff=" + nanoDiff);
        log.info(kind + " item=" + i + " item/s=" + ((i *10^4)/ millisDiff));
    }
}