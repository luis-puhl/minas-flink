package br.ufscar.dc.gsdr.mfog.util;

import br.ufscar.dc.gsdr.mfog.structs.Point;

import java.io.*;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;

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
                log.info("new reader");
                ObjectInputStream reader = new ObjectInputStream(new BufferedInputStream(inputStream));
                // ObjectOutputStream writer = new ObjectOutputStream(new BufferedOutputStream(outputStream)); // causes deadlock
                for (int j = 0; j < 3; j++) {
                    log.info("readObject");
                    Object fromBytes = reader.readObject();
                    log.info(fromBytes);
                }
                //
                log.info("new writer");
                ObjectOutputStream writer = new ObjectOutputStream(new BufferedOutputStream(outputStream));
                for (int j = 0; j < 3; j++) {
                    log.info("writeObject");
                    writer.writeObject(Point.zero(22));
                    log.info("flush");
                    writer.flush();
                }

                // Let user know you wrote to socket
                log.info("[Client] Hello " + nbrTry++ + " !! ");
            }
        }
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
                log.info("new writer");
                ObjectOutputStream writer = new ObjectOutputStream(new BufferedOutputStream(outputStream));
                //
                for (int j = 0; j < 3; j++) {
                    log.info("write");
                    writer.writeObject(Point.zero(22));
                    log.info("flush");
                    writer.flush();
                }
                //
                log.info("new reader");
                ObjectInputStream reader = new ObjectInputStream(new BufferedInputStream(inputStream));
                for (int j = 0; j < 3; j++) {
                    log.info("readObject");
                    Point fromBytes = (Point) reader.readObject();
                    log.info(fromBytes);
                }

                reader.close();
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