package br.ufscar.dc.gsdr.mfog.util;

import br.ufscar.dc.gsdr.mfog.structs.Point;

import java.io.*;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.zip.GZIPInputStream;
import java.util.zip.GZIPOutputStream;

public class ServerClient {

    static class MyClient {
        public void main() throws IOException, ClassNotFoundException {
            int nbrTry = 0;
            Logger log = Logger.getLogger("Client");
            for (int i = 0; i < 3; i++) {
                log.info("socket");
                Socket socket = new Socket(InetAddress.getByName("localhost"), 15243);
                log.info("outputStream");
                OutputStream outputStream = socket.getOutputStream();
                log.info("inputStream");
                InputStream inputStream = socket.getInputStream();
                //
                receive(log, inputStream);
                //
                send(log, outputStream);

                // Let user know you wrote to socket
                log.info("[Client] Hello " + nbrTry++ + " !! ");
            }
        }
    }

    private static void receive(Logger log, InputStream inputStream) throws IOException {
        log.info("new reader");
        DataInputStream reader = new DataInputStream(new GZIPInputStream(new BufferedInputStream(inputStream)));
        // ObjectOutputStream writer = new ObjectOutputStream(new BufferedOutputStream(outputStream)); // causes deadlock
        for (int j = 0; j < 3; j++) {
            log.info("readObject");
            Point fromBytes = Point.fromDataInputStream(reader);
            // Point fromBytes = reader.readObject();
            log.info(fromBytes);
        }
    }

    private static void send(Logger log, OutputStream outputStream) throws IOException {
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

    static class MyServer {
        public void main() throws IOException, ClassNotFoundException {
            Logger log = Logger.getLogger("Server");
            ServerSocket server = new ServerSocket(15243, 0, InetAddress.getByName("localhost"));
            for (int i = 0; i < 3; i++) {
                log.info("accept");
                Socket socket = server.accept();
                log.info("outputStream");
                OutputStream outputStream = socket.getOutputStream();
                log.info("inputStream");
                InputStream inputStream = socket.getInputStream();
                //
                send(log, outputStream);
                receive(log, inputStream);
                //
                socket.close();
            }
        }
    }

    public static void main(String[] args) throws InterruptedException {
        Thread server = new Thread(() -> {
            try {
                new MyServer().main();
            } catch (IOException | ClassNotFoundException e) {
                e.printStackTrace();
            }
        });
        server.start();
        Thread client = new Thread(() -> {
            try {
                new MyClient().main();
            } catch (IOException | ClassNotFoundException e) {
                e.printStackTrace();
            }
        });
        client.start();
        client.join();
        server.join();
    }
}