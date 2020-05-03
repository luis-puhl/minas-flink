package br.ufscar.dc.gsdr.mfog.util;

import br.ufscar.dc.gsdr.mfog.structs.Point;

import java.io.*;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.ArrayList;
import java.util.List;
import java.util.zip.GZIPInputStream;
import java.util.zip.GZIPOutputStream;

public class ServerClientExperiments<T extends ServerClientExperiments.SendReceive> {
    interface SendReceive {
        void receive(InputStream inputStream) throws IOException;
        void send(OutputStream outputStream) throws IOException;
    }
    static class GzipSendReceive implements SendReceive {
        Logger log = Logger.getLogger(GzipSendReceive.class);
        public void receive(InputStream inputStream) throws IOException {
            log.info("new reader");
            DataInputStream reader = new DataInputStream(new GZIPInputStream(new BufferedInputStream(inputStream)));
            for (int j = 0; j < 3; j++) {
                log.info("readObject");
                Point fromBytes = Point.fromDataInputStream(reader);
                log.info("read: " + fromBytes);
            }
        }

        public void send(OutputStream outputStream) throws IOException {
            log.info("new writer");
            GZIPOutputStream gzipOut = new GZIPOutputStream(new BufferedOutputStream(outputStream));
            DataOutputStream writer = new DataOutputStream(gzipOut);
            for (int j = 0; j < 3; j++) {
                log.info("writeObject");
                Point.zero(22).toDataOutputStream(writer);
            }
            log.info("flush");
            writer.flush();
            gzipOut.finish();
            gzipOut.flush();
            outputStream.flush();
        }
    }

    static class ObjectSendReceive implements SendReceive {
        Logger log = Logger.getLogger(ObjectSendReceive.class);
        public void receive(InputStream inputStream) throws IOException {
            log.info("new reader");
            ObjectInputStream reader = new ObjectInputStream(new BufferedInputStream(inputStream));
            for (int j = 0; j < 3; j++) {
                log.info("readObject");
                try {
                    Point fromBytes = (Point) reader.readObject();
                    log.info("read: " + fromBytes);
                } catch (ClassNotFoundException e) {
                    log.error(e);
                }
            }
        }

        public void send(OutputStream outputStream) throws IOException {
            log.info("new writer");
            ObjectOutputStream writer = new ObjectOutputStream(new BufferedOutputStream(outputStream));
            //
            for (int j = 0; j < 3; j++) {
                log.info("write");
                writer.writeObject(Point.zero(22));
            }
            log.info("flush");
            writer.flush();
            outputStream.flush();
        }
    }

    static class JsonSendReceive implements SendReceive {
        Logger log = Logger.getLogger(JsonSendReceive.class);
        public void receive(InputStream inputStream) throws IOException {
            log.info("new reader");
            BufferedReader reader = new BufferedReader(new InputStreamReader(inputStream));
            for (int j = 0; j < 3; j++) {
                log.info("readJson");
                String line;
                do {
                    line = reader.readLine() ;
                } while (line == null);
                Point fromBytes = Point.fromJson(line);
                log.info("read: " + fromBytes);
            }
        }

        public void send(OutputStream outputStream) throws IOException {
            log.info("new writer");
            BufferedWriter writer = new BufferedWriter(new OutputStreamWriter(outputStream));
            //
            for (int j = 0; j < 3; j++) {
                log.info("writeObject");
                writer.write(Point.zero(22).json().toString());
                writer.newLine();
            }
            log.info("flush");
            writer.flush();
            outputStream.flush();
        }
    }

    protected Logger log;
    protected T sendReceive;
    protected final Class<T> typeParameterClass;
    ServerClientExperiments(Class<T> typeParameterClass) throws Exception {
        this.typeParameterClass = typeParameterClass;
        this.sendReceive = typeParameterClass.getDeclaredConstructor().newInstance();
        this.log = Logger.getLogger(this.getClass(), typeParameterClass);
    }

    void symmetric(Socket socket) throws IOException {
        log.info("outputStream");
        OutputStream outputStream = socket.getOutputStream();
        log.info("inputStream");
        InputStream inputStream = socket.getInputStream();
        if (inputStream.available() > 1){
            sendReceive.receive(inputStream);
            sendReceive.send(outputStream);
        } else {
            sendReceive.send(outputStream);
            sendReceive.receive(inputStream);
        }
    }
    ServerSocket server;
    public void server() throws IOException {
        server = new ServerSocket(15243, 0, InetAddress.getByName("localhost"));
    }
    public void serverAccept() throws IOException {
        for (int i = 0; i < 3; i++) {
            log.info("accept");
            Socket socket = server.accept();
            symmetric(socket);
            log.info("Server Hello " + i);
            socket.close();
        }
        server.close();
    }
    public void client() throws IOException {
        for (int i = 0; i < 3; i++) {
            log.info("socket");
            Socket socket = new Socket(InetAddress.getByName("localhost"), 15243);
            symmetric(socket);
            log.info("Client Hello " + i);
            socket.close();
        }
    }

    public static void main(String[] args) throws Exception {
        Logger log = Logger.getLogger(ServerClientExperiments.class);
        boolean isServer = args.length > 0;
        String kind = "client";
        if (isServer) {
            kind = "server";
        }
        log.info("Self test >" + kind);

        Logger.filterServices.add(GzipSendReceive.class.getSimpleName());
        Logger.filterServices.add(ObjectSendReceive.class.getSimpleName());
        Logger.filterServices.add(JsonSendReceive.class.getSimpleName());
        Logger.filterServices.add(ServerClientExperiments.class.getSimpleName());

        List<Class<? extends SendReceive>> transmitters = new ArrayList<>(3);
        transmitters.add(JsonSendReceive.class);
        transmitters.add(ObjectSendReceive.class);
        transmitters.add(GzipSendReceive.class);

        for (Class<? extends SendReceive> transmitter : transmitters) {
            ServerClientExperiments<? extends SendReceive> serverClient = new ServerClientExperiments<>(transmitter);
            if (isServer) {
                serverClient.server();
            }
            log.info("Will run with " + transmitter.getSimpleName());
            long start = System.currentTimeMillis();
            long diff;
            do {
                diff = System.currentTimeMillis() - start;
                Thread.sleep(diff / 2);
            } while (diff < 1000 );
            if (!isServer) Thread.sleep(100);

            long millis = System.currentTimeMillis();
            long nano = System.nanoTime();
            if (isServer) {
                serverClient.serverAccept();
            } else {
                serverClient.client();
            }
            long millisDiff = System.currentTimeMillis() - millis;
            long nanoDiff = System.nanoTime() - nano;
            log.info(kind + " millisDiff=" + millisDiff + " nanoDiff=" + nanoDiff);
        }
    }
}