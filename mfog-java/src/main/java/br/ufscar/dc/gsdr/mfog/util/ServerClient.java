package br.ufscar.dc.gsdr.mfog.util;

import br.ufscar.dc.gsdr.mfog.structs.Point;
import br.ufscar.dc.gsdr.mfog.structs.WithSerializable;

import java.io.*;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

public class ServerClient<T extends WithSerializable<T>> {
    public static final int PORT = 9999;
    protected Logger log;
    protected GzipSendReceive<T> sendReceive;
    protected final Class<T> typeParameterClass;
    T reusableObject;
    ServerClient(Class<T> typeParameterClass) throws Exception {
        this.reusableObject = typeParameterClass.getDeclaredConstructor().newInstance();
        this.typeParameterClass = typeParameterClass;
        this.sendReceive = new GzipSendReceive<>();
        this.log = Logger.getLogger(this.getClass(), typeParameterClass);
    }

    ServerSocket serverSocket;
    Socket socket;
    OutputStream outputStream;
    InputStream inputStream;
    public void server() throws IOException {
        serverSocket = new ServerSocket(PORT, 0, InetAddress.getByName("localhost"));
    }
    public void serverAccept() throws IOException {
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
                socket = new Socket("localhost", PORT);
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
    int sender(Iterator<WithSerializable<T>> toSend) throws IOException {
        return sendReceive.send(outputStream, toSend);
    }
    T receive(T reusableObject) throws IOException {
        return sendReceive.receive(inputStream, reusableObject);
    }


    public static void main(String[] args) throws Exception {
        Logger log = Logger.getLogger(ServerClient.class);
        Logger.filterServices.add(GzipSendReceive.class.getSimpleName());
        boolean isServer = args.length > 0;
        String kind = "client";
        if (isServer) {
            kind = "server";
        }
        log.info("Self test >" + kind);

        List<WithSerializable<Point>> points = new ArrayList<>(653457);
        for (int i = 0; i < 653457; i++) {
            Point zero = Point.zero(22);
            zero.id = i;
            points.add(zero);
        }
        log.info("Point List full ");
        ServerClient<Point> serverClient = new ServerClient<>(Point.class);
        if (isServer) {
            serverClient.server();
        }
        //
        int i = 0;
        long millis = System.currentTimeMillis();
        long nano = System.nanoTime();
        if (isServer) {
            serverClient.serverAccept();
            i = serverClient.sender(points.iterator());
            log.info(kind + " socket.close()");
            serverClient.socket.close();
            serverClient.serverSocket.close();
        } else {
            serverClient.client();
            Thread.sleep(1000);
            Point reuse = new Point();
            while (serverClient.socket.isConnected() && !serverClient.socket.isInputShutdown()){
                try {
                    reuse = serverClient.receive(reuse);
                } catch (java.io.EOFException e) {
                    break;
                }
                i++;
//                if (i % 1000 == 0) {
//                    System.out.print(".");
//                }
            }
            log.info(kind + " socket.close()");
            serverClient.socket.close();
        }
        long millisDiff = System.currentTimeMillis() - millis;
        long nanoDiff = System.nanoTime() - nano;
        log.info(kind + " millisDiff=" + millisDiff + " nanoDiff=" + nanoDiff);
        log.info(kind + " item=" + i + " item/s=" + ((i *10^4)/ millisDiff));
    }
}