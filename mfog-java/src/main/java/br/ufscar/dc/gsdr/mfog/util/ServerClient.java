package br.ufscar.dc.gsdr.mfog.util;

import br.ufscar.dc.gsdr.mfog.structs.Point;

import java.io.*;
import java.lang.reflect.InvocationTargetException;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.ArrayList;
import java.util.List;
import java.util.zip.GZIPInputStream;
import java.util.zip.GZIPOutputStream;

public class ServerClient {
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

    static abstract class Base<T extends SendReceive> {
        protected Logger log;
        protected T sendReceive;
        protected final Class<T> typeParameterClass;
        Base(T sendReceive, Class<T> typeParameterClass) {
            this.typeParameterClass = typeParameterClass;
            this.sendReceive = sendReceive;
            log = Logger.getLogger(this.getClass(), typeParameterClass);
        }
        abstract void main() throws IOException;
        void timeIt() throws IOException {
            Logger log = Logger.getLogger(Base.class);
            long millis = System.currentTimeMillis();
            long nano = System.nanoTime();
            main();
            long millisDiff = System.currentTimeMillis() - millis;
            long nanoDiff = System.nanoTime() - nano;
            log.info("millisDiff=" + millisDiff + " nanoDiff=" + nanoDiff);
        }
    }
    static class Server<T extends SendReceive> extends Base<T> {
        ServerSocket server;
        Server(Class<T> sendReceive) throws IOException, NoSuchMethodException, IllegalAccessException, InvocationTargetException, InstantiationException {
            super(sendReceive.getDeclaredConstructor().newInstance(), sendReceive);
            server = new ServerSocket(15243, 0, InetAddress.getByName("localhost"));
        }

        public void main() throws IOException {
            for (int i = 0; i < 3; i++) {
                log.info("accept");
                Socket socket = server.accept();
                log.info("outputStream");
                OutputStream outputStream = socket.getOutputStream();
                log.info("inputStream");
                InputStream inputStream = socket.getInputStream();
                //
                sendReceive.send(outputStream);
                sendReceive.receive(inputStream);
                //
                socket.close();
            }
            server.close();
        }
    }

    static class Client<T extends SendReceive> extends Base<T> {
        Socket socket;
        Client(Class<T> sendReceive) throws IOException, NoSuchMethodException, IllegalAccessException, InvocationTargetException, InstantiationException {
            super(sendReceive.getDeclaredConstructor().newInstance(), sendReceive);
        }

        public void main() throws IOException {
            for (int i = 0; i < 3; i++) {
                log.info("socket");
                socket = new Socket(InetAddress.getByName("localhost"), 15243);
                log.info("outputStream");
                OutputStream outputStream = socket.getOutputStream();
                log.info("inputStream");
                InputStream inputStream = socket.getInputStream();
                sendReceive.receive(inputStream);
                sendReceive.send(outputStream);
                // Let user know you wrote to socket
                log.info("Hello " + i);
                log.info("socket.close()");
                socket.close();
            }
        }
    }

    void serverRunner() throws Exception {
        for (Class<? extends SendReceive> transmitter : transmitters) {
            Server<? extends SendReceive> server = new Server<>(transmitter);
            waitSync();
            server.timeIt();
        }
    }
    void clientRunner() throws Exception {
        for (Class<? extends SendReceive> transmitter : transmitters) {
            waitSync();
            Client<? extends SendReceive> client = new Client<>(transmitter);
            client.timeIt();
        }
    }
    void waitSync() {
        long start = System.currentTimeMillis();
        while (System.currentTimeMillis() - start < 1000) {
            try {
                Thread.sleep(100);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }
    }
    List<Class<? extends SendReceive>> transmitters;
    void init() {
        Logger.filterServices.add(GzipSendReceive.class.getSimpleName());
        Logger.filterServices.add(ObjectSendReceive.class.getSimpleName());
        Logger.filterServices.add(JsonSendReceive.class.getSimpleName());
        Logger.filterServices.add(Client.class.getSimpleName());
        Logger.filterServices.add(Server.class.getSimpleName());
        transmitters = new ArrayList<>(3);
        transmitters.add(JsonSendReceive.class);
        transmitters.add(ObjectSendReceive.class);
        transmitters.add(GzipSendReceive.class);
    }

    public static void main(String[] args) throws Exception {
        ServerClient serverClient = new ServerClient();
        serverClient.init();
        if (args.length > 0) {
            serverClient.serverRunner();
        } else {
            serverClient.clientRunner();
        }
    }
}