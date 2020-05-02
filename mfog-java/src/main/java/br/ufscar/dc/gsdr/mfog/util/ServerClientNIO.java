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
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Iterator;
import java.util.Set;
import java.util.zip.GZIPInputStream;
import java.util.zip.GZIPOutputStream;

public class ServerClientNIO {

    static class MyClient {
        public void main() throws IOException, ClassNotFoundException {
            Logger log = Logger.getLogger("Client");
            log.info("new Compressor");
            Compressor compressor = new Compressor(256);
            //
            log.info("new messages");
            Iterator<Point> messages = Arrays.asList((new Point[]{
                    Point.zero(22), Point.zero(22), Point.zero(22)
            })).iterator();
            //
            for (int i = 0; i < 3; i++) {
                log.info("Connecting to Server on port 1111...");
                SocketChannel socketChannel = SocketChannel.open(new InetSocketAddress("localhost", 1111));
                while (messages.hasNext()) {
                    Point next = messages.next();
                    compressor.encode(next);
                    socketChannel.write(compressor.byteBuffer);
                }
                compressor.byteBuffer.clear();
                for (int j = 0; j < 3; j++) {
                    socketChannel.read(compressor.byteBuffer);
                    Point message = (Point) compressor.decode();
                    log.info("Message received: " + message);
                }
                //
                socketChannel.close();
                log.info("[Client] Hello " + i++ + " !! ");
            }
        }
    }

    static class MyServer {
        public void main() throws IOException, ClassNotFoundException {
            Logger log = Logger.getLogger("Server");
            log.info("new Compressor");
            Compressor compressor = new Compressor(256);
            //
            log.info("Selector.open");
            Selector selector = Selector.open();
            log.info("ServerSocketChannel.open");
            ServerSocketChannel serverSocketChannel = ServerSocketChannel.open();
            log.info("serverSocketChannel.bind");
            serverSocketChannel.bind(new InetSocketAddress("localhost", 1111));
            serverSocketChannel.configureBlocking(false);
            serverSocketChannel.register(selector, serverSocketChannel.validOps(), null);
            //
            Iterator<Point> messages = Arrays.asList((new Point[]{
                    Point.zero(22), Point.zero(22), Point.zero(22)
            })).iterator();
            for (int i = 0; i < 3; i++) {
                log.info("select");
                selector.select();
                Set<SelectionKey> selectedKeys = selector.selectedKeys();
                Iterator<SelectionKey> keyIterator = selectedKeys.iterator();
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
                    if (myKey.isWritable() && messages.hasNext()) {
                        SocketChannel socketChannel = (SocketChannel) myKey.channel();
                        Point message = messages.next();
                        compressor.encode(message);
                        socketChannel.write(compressor.byteBuffer);
                        log.info("Message sent: " + message);
                    }
                    keyIterator.remove();
                }
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