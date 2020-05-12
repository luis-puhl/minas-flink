package br.ufscar.dc.gsdr.mfog;


import com.esotericsoftware.kryo.Kryo;
import com.esotericsoftware.kryo.Serializer;
import com.esotericsoftware.kryo.io.ByteBufferInputStream;
import com.esotericsoftware.kryo.io.ByteBufferOutputStream;
import com.esotericsoftware.kryo.io.Input;
import com.esotericsoftware.kryo.io.Output;
import com.esotericsoftware.kryonet.Server;

import java.io.IOException;
import java.io.PipedInputStream;
import java.net.InetSocketAddress;
import java.nio.ByteBuffer;
import java.nio.channels.SelectionKey;
import java.nio.channels.Selector;
import java.nio.channels.ServerSocketChannel;
import java.nio.channels.SocketChannel;
import java.util.*;

public class RelayService<T> {
    protected final int port;
    protected final Logger log;
    protected final Class<T> typeParameterClass;
    protected final Serializer<T> serializer;
    protected final Kryo kryo;

    RelayService(Class<T> typeParameterClass, Class<?> caller, int port, Kryo kryo, Serializer<T> serializer) {
        this.typeParameterClass = typeParameterClass;
        this.port = port;
        this.log = Logger.getLogger(this.getClass(), typeParameterClass, caller);
        this.kryo = kryo;
        this.serializer = serializer;
    }

    class Connection {
        public SocketChannel socketChannel;
        public Iterator<T> toSend;
        //
        public ByteBuffer outputByteBuffer = ByteBuffer.allocate(10 * 1024);
        public ByteBufferOutputStream byteBufferOutputStream = new ByteBufferOutputStream(outputByteBuffer);
        public Output output = new Output(byteBufferOutputStream);
        //
        public ByteBuffer inputByteBuffer = ByteBuffer.allocate(10 * 1024);
        public ByteBufferInputStream byteBufferInputStream = new ByteBufferInputStream(ByteBuffer.allocate(10 * 1024));
        public Input input = new Input(byteBufferInputStream);
        Connection(SocketChannel socketChannel, Iterator<T> toSend) {
            this.socketChannel = socketChannel;
            this.toSend = toSend;
        }
    }
    List<T> store = new LinkedList<>();
    Map<SocketChannel, Connection> connections = new HashMap<>();
    public void run() throws IOException {
        Selector selector = Selector.open();
        ServerSocketChannel serverSocketChannel = ServerSocketChannel.open();
        serverSocketChannel.bind(new InetSocketAddress(port));
        serverSocketChannel.configureBlocking(false);
        serverSocketChannel.register(selector, serverSocketChannel.validOps(), null);
        //
        log.info("Server Ready");
        while (serverSocketChannel.isOpen()) {
            selector.select();
            for (SelectionKey selectedKey : selector.selectedKeys()) {
                if (!selectedKey.isValid()) {
                    log.info("Invalid key: " + selectedKey);
                    continue;
                }
                if (selectedKey.isAcceptable()) {
                    ServerSocketChannel selectedServer = (ServerSocketChannel) selectedKey.channel();
                    SocketChannel client = selectedServer.accept();
                    if (client == null) {
                        // log.info("Invalid client: " + Integer.toBinaryString(selectedKey.readyOps()));
                        // log.info("failed to accept");
                        continue;
                    }
                    client.configureBlocking(false); // java.lang.NullPointerException
                    client.register(selector, client.validOps());
                    // client.register(selector, SelectionKey.OP_WRITE);
                    connections.put(client, new Connection(client, store.iterator()));
                    log.info("Connection Accepted: " + client.getLocalAddress());
                }
                if (selectedKey.isReadable() || selectedKey.isWritable()) {
                    SocketChannel client = (SocketChannel) selectedKey.channel();
                    if (client == null) {
                        log.info("Invalid client: " + selectedKey);
                        continue;
                    }
                    if (selectedKey.isReadable()) {
                        Connection connection = connections.get(client);
                        int read = client.read(connection.inputByteBuffer);
                        //
                        PipedInputStream pipedInputStream = new PipedInputStream();
//                        pipedInputStream.
//                        connection.inputByteBuffer.flip().duplicate()
                        connection.input.setBuffer(connection.inputByteBuffer.array());
                        try {
                            T next = serializer.read(kryo, connection.input, typeParameterClass);
                            store.add(next);
                            connection.inputByteBuffer.clear();
                            log.info("Message received: " + next);
                        } catch (com.esotericsoftware.kryo.KryoException e) {
                            log.warn(e.getMessage());
                            client.close();
                            serverSocketChannel.close();
                            break;
                        }
                    }
                    if (selectedKey.isWritable()) {
                        Connection connection = connections.get(client);
                        if (connection.toSend.hasNext()) {
                            T next = connection.toSend.next();
                            serializer.write(kryo, connection.output, next);
                            connection.output.flush();
                            client.write(connection.byteBufferOutputStream.getByteBuffer());
                            log.info("Message sent: " + next);
                        }
                    }
                }
            }
            // selector.keys().clear();
            // java.lang.UnsupportedOperationException
            //   at java.util.Collections$UnmodifiableCollection.clear(Collections.java:1076)
            //   at br.ufscar.dc.gsdr.mfog.RelayService.run(RelayService.java:89)
        }
        serverSocketChannel.close();
    }
}
