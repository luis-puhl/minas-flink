package br.ufscar.dc.gsdr.mfog.util;

import br.ufscar.dc.gsdr.mfog.structs.SelfDataStreamSerializable;
import com.esotericsoftware.kryo.Kryo;
import com.esotericsoftware.kryo.Serializer;
import com.esotericsoftware.kryo.io.Input;
import com.esotericsoftware.kryo.io.Output;

import java.io.Closeable;
import java.io.IOException;
import java.net.InetSocketAddress;
import java.nio.ByteBuffer;
import java.nio.channels.*;

public class TcpNIO<T extends Serializer<T>> implements Closeable {
    protected final Class<T> typeParameterClass;
    protected final Logger log;
    protected ServerSocketChannel serverSocket;
    protected Selector selector;

    public TcpNIO(Class<T> typeParameterClass, Class<?> caller) throws IOException {
        this.typeParameterClass = typeParameterClass;
        this.log = Logger.getLogger(this.getClass(), typeParameterClass, caller);
    }

    public void server(int port) throws IOException {
        if (serverSocket != null && serverSocket.isOpen()) {
            serverSocket.close();
        }
        serverSocket = ServerSocketChannel.open();
        InetSocketAddress address = new InetSocketAddress(port);
        serverSocket.bind(address);
        serverSocket.configureBlocking(false);
        int ops = serverSocket.validOps();
        serverSocket.register(selector, ops, null);
        log.info("server ready on " + address.toString());
    }

    public void client(String hostname, int port) throws IOException {
        InetSocketAddress address = new InetSocketAddress(hostname, port);
        SocketChannel client = SocketChannel.open(address);
        client.configureBlocking(false);
        client.register(selector, SelectionKey.OP_READ);
        log.info("Connection Accepted: " + client.getLocalAddress() + "\n");
    }

    protected T toReceive;
    protected T toSend;
    private static final int BUFFER_SIZE = 10 * 1024;
    protected ByteBuffer outBuffer = ByteBuffer.allocate(BUFFER_SIZE);
    protected Output out = new Output(BUFFER_SIZE);
    protected ByteBuffer inBuffer = ByteBuffer.allocate(BUFFER_SIZE);
    protected Input in = new Input(BUFFER_SIZE);
    protected Kryo kryo = new Kryo();
    public void sendReceive() throws IOException {
        selector.select();
        for (SelectionKey selectedKey : selector.selectedKeys()) {
            if (!selectedKey.isValid()) continue;
            if (serverSocket != null && selectedKey.isAcceptable()) {
                SocketChannel client = serverSocket.accept();
                client.configureBlocking(false);
                client.register(selector, SelectionKey.OP_READ);
                log.info("Connection Accepted: " + client.getLocalAddress() + "\n");
            }
            if (toReceive != null && selectedKey.isReadable()) {
                SocketChannel client = (SocketChannel) selectedKey.channel();
                inBuffer.clear();
                client.read(inBuffer);
                toReceive.read(kryo, in, typeParameterClass);
                String result = new String(outBuffer.array()).trim();
                log.info("Message received: " + result);
            }
            if (toSend != null && selectedKey.isWritable()) {
                SocketChannel client = (SocketChannel) selectedKey.channel();
                toSend.write(kryo, out, toSend);
                out.flush();
                client.write(outBuffer);
                out.clear();
                toSend = null;
            }
        }
        selector.keys().clear();
    }

    @Override
    public void close() throws IOException {

    }
}
