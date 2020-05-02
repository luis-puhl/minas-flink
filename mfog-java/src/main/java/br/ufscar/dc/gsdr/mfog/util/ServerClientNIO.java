package br.ufscar.dc.gsdr.mfog.util;

import br.ufscar.dc.gsdr.mfog.structs.Point;

import java.io.*;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.nio.channels.SelectionKey;
import java.nio.channels.Selector;
import java.nio.channels.ServerSocketChannel;
import java.nio.channels.SocketChannel;
import java.util.Iterator;
import java.util.Set;
import java.util.zip.GZIPInputStream;
import java.util.zip.GZIPOutputStream;

public class ServerClientNIO {

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
                ObjectInputStream reader = new ObjectInputStream(new GZIPInputStream(inputStream));
                // ObjectOutputStream writer = new ObjectOutputStream(new GZIPOutputStream(outputStream)); // causes deadlock
                for (int j = 0; j < 3; j++) {
                    log.info("readObject");
                    Object fromBytes = reader.readObject();
                    log.info(fromBytes);
                }
                //
                log.info("new writer");
                ObjectOutputStream writer = new ObjectOutputStream(new GZIPOutputStream(outputStream));
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
            Selector selector = Selector.open();
            ServerSocketChannel serverSocketChannel = ServerSocketChannel.open();
            serverSocketChannel.bind(new InetSocketAddress("localhost", 1111));
            serverSocketChannel.configureBlocking(false);
            serverSocketChannel.register(selector, serverSocketChannel.validOps(), null);
            //
            // ServerSocket server = new ServerSocket(15243, 0, InetAddress.getByName("localhost"));
            Compressor compressor = new Compressor(256);
            log.info("select");
            selector.select();
            Set<SelectionKey> selectedKeys = selector.selectedKeys();
            Iterator<SelectionKey> keyIterator = selectedKeys.iterator();
            for (int i = 0; i < 3; i++) {

                while (keyIterator.hasNext()) {
                    SelectionKey myKey = keyIterator.next();
                    if (myKey.isAcceptable()) {
                        log.info("accept");
                        SocketChannel socketChannel = serverSocketChannel.accept();
                        socketChannel.configureBlocking(false);
                        socketChannel.register(selector, SelectionKey.OP_READ);
                        log.info("Connection Accepted: " + socketChannel.getLocalAddress() + "\n");
                    }
                    if (myKey.isReadable()) {
                        SocketChannel socketChannel = (SocketChannel) myKey.channel();
                        socketChannel.read(compressor.byteBuffer);
                        Point message = (Point) compressor.decode();
                        log.info("Message received: " + message);
                    }
                    if (myKey.isWritable()) {
                        SocketChannel socketChannel = (SocketChannel) myKey.channel();
                        Point message = Point.zero(22);
                        compressor.encode(message);
                        socketChannel.write(compressor.byteBuffer);
                        log.info("Message sent: " + message);
                    }
                    keyIterator.remove();
                }
                //
                log.info("new writer");
                GZIPOutputStream writerGzip = new GZIPOutputStream(socket.getOutputStream());
                ObjectOutputStream writer = new ObjectOutputStream(writerGzip);
                log.info("new reader");
                ObjectInputStream reader = new ObjectInputStream(new GZIPInputStream(socket.getInputStream()));
                //
                for (int j = 0; j < 3; j++) {
                    log.info("write");
                    writer.writeObject(Point.zero(22));
                }
                log.info("flush");
                writerGzip.flush();
                writerGzip.close();
                //
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