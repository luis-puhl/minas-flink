package br.ufscar.dc.gsdr.mfog.util;

import java.io.IOException;
import java.net.InetSocketAddress;
import java.nio.ByteBuffer;
import java.nio.CharBuffer;
import java.nio.channels.SelectionKey;
import java.nio.channels.Selector;
import java.nio.channels.ServerSocketChannel;
import java.nio.channels.SocketChannel;
import java.nio.charset.Charset;
import java.nio.charset.CharsetDecoder;
import java.nio.charset.CharsetEncoder;
import java.nio.charset.StandardCharsets;
import java.util.Date;
import java.util.Iterator;

public final class WebServer extends Thread {
    private ByteBuffer buf = ByteBuffer.allocate(2048);
    private Charset charset = StandardCharsets.UTF_8;
    private CharsetDecoder decoder = charset.newDecoder();
    private CharsetEncoder encoder = charset.newEncoder();
    private Selector selector = Selector.open();
    private ServerSocketChannel server = ServerSocketChannel.open();
    private boolean isRunning = true;
    public WebServer(InetSocketAddress address) throws IOException {
        super("Web Server - " + address);
        server.socket().bind(address);
        server.configureBlocking(false);
        server.register(selector, SelectionKey.OP_ACCEPT);
    }
    public static void main(String[] args) throws Exception {
        new WebServer(new InetSocketAddress(5555)).start();
    }
    @Override
    public void run() {
        try {
            while (isRunning) {
                selector.select();
                Iterator<SelectionKey> i = selector.selectedKeys().iterator();
                while (i.hasNext()) {
                    SelectionKey key = i.next();
                    i.remove();
                    if (!key.isValid()) {
                        continue;
                    }
                    try {
                        // get a new connection
                        if (key.isAcceptable()) {
                            // accept them
                            SocketChannel client = server.accept();
                            // non blocking please
                            client.configureBlocking(false);
                            // show out intentions
                            client.register(selector, SelectionKey.OP_READ);
                            // read from the connection
                        } else if (key.isReadable()) {
                            // get the client
                            SocketChannel client = (SocketChannel) key.channel();
                            // reset our buffer
                            buf.rewind();
                            // read into it
                            client.read(buf);
                            // flip it so we can decode it
                            buf.flip();
                            // decode the bytes, handle it, and write the response
                            client.write(encoder.encode(CharBuffer.wrap(
                                "HTTP/1.1 200 OK\r\n\r\n" + handle(client, decoder.decode(buf).toString()) + "\r\n")));
                        }
                    } catch (Exception ex) {
                        System.err.println("Error handling client: " + key.channel());
                        System.err.println(ex);
                        System.err.println("\tat " + ex.getStackTrace()[0]);
                    } finally {
                        if (key.channel() instanceof SocketChannel) {
                            key.channel().close();
                        }
                    }
                }
            }
        } catch (IOException ex) {
            ex.printStackTrace();
        } finally {
            shutdown();
        }
    }
    private String handle(SocketChannel client, String request) {
        return "I liek cats. The current time is: " + new Date();
    }
    public void shutdown() {
        isRunning = false;
        try {
            selector.close();
            server.close();
        } catch (IOException ex) {
            // do nothing, its game over
        }
        buf = null;
        charset = null;
        decoder = null;
        encoder = null;
        selector = null;
        server = null;
    }
}
