package br.ufscar.dc.gsdr.mfog.util;

import com.esotericsoftware.kryo.Kryo;
import com.esotericsoftware.kryo.io.ByteBufferInput;
import com.esotericsoftware.kryo.io.ByteBufferOutput;
import org.slf4j.LoggerFactory;

import java.io.Closeable;
import java.io.IOException;
import java.net.InetSocketAddress;
import java.net.SocketException;
import java.nio.ByteBuffer;
import java.nio.channels.*;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Set;

import static com.esotericsoftware.minlog.Log.*;

public class KryoNet {

    public static class Connection implements Closeable {
        public final int writeBufferSize;
        public final int objectBufferSize;
        public final Server endPoint;
        public final ByteBuffer writeBuffer;
        public final ByteBuffer readBuffer;
        public final SocketChannel socketChannel;
        public final Kryo kryo;
        public final ByteBufferInput input;
        public final ByteBufferOutput output;
        public final int id;
        public long lastReadTime, lastWriteTime;
        public int currentObjectLength;
        public final SelectionKey selectionKey;

        public Connection(
            int writeBufferSize, int objectBufferSize, Server endPoint,
            SocketChannel socketChannel, Selector selector, int id, Kryo kryo
        ) throws IOException {
            this.writeBufferSize = writeBufferSize;
            this.objectBufferSize = objectBufferSize;
            this.endPoint = endPoint;
            this.writeBuffer = ByteBuffer.allocate(writeBufferSize);
            this.readBuffer = ByteBuffer.allocate(objectBufferSize);
            this.readBuffer.flip();
            assert socketChannel != null;
            this.socketChannel = socketChannel;
            this.id = id;
            this.lastReadTime = this.lastWriteTime = System.currentTimeMillis();
            this.currentObjectLength = 0;
            //
            this.socketChannel.configureBlocking(false);
            this.socketChannel.socket().setTcpNoDelay(true);
            this.selectionKey = this.socketChannel.register(selector, SelectionKey.OP_READ);
            this.selectionKey.attach(this);
            //
            this.kryo = kryo;
            /*
            this.kryo.register(RegisterTCP.class);
            this.kryo.register(RegisterUDP.class);
            this.kryo.register(KeepAlive.class);
            this.kryo.register(DiscoverHost.class);
            this.kryo.register(Ping.class);
             */
            this.input = new ByteBufferInput();
            this.output = new ByteBufferOutput();
        }

        public synchronized void write(Object object) {
            output.setBuffer(writeBuffer);
            // kryo.getContext().put("connection", connection);
            kryo.writeClassAndObject(output, object);
            output.flush();
        }
        public int send(Object object) throws IOException {
            if (socketChannel == null) throw new SocketException("Connection is closed.");
            synchronized (socketChannel) {
                // Leave room for length.
                int start = writeBuffer.position();
                int lengthLength = getLengthLength();
                writeBuffer.position(writeBuffer.position() + lengthLength);

                // Write data.
                try {
                    write(object);
                } catch (Exception ex) {
                    throw new IOException("Error serializing object of type: " + object.getClass().getName(), ex);
                }
                int end = writeBuffer.position();

                // Write data length.
                writeBuffer.position(start);
                writeLength(writeBuffer, end - lengthLength - start);
                writeBuffer.position(end);

                // Write to socket if no data was queued.
                if (start == 0 && !writeToSocket()) {
                    // A partial write, set OP_WRITE to be notified when more writing can occur.
                    selectionKey.interestOps(SelectionKey.OP_READ | SelectionKey.OP_WRITE);
                } else {
                    // Full write, wake up selector so idle event will be fired.
                    selectionKey.selector().wakeup();
                }

                float percentage = writeBuffer.position() / (float)writeBuffer.capacity();
                lastWriteTime = System.currentTimeMillis();
                return end - start;
            }
        }

        public void writeLength(ByteBuffer buffer, int length) {
            buffer.putInt(length);
        }

        public void writeOperation () throws IOException {
            synchronized (selectionKey) {
                if (writeToSocket()) {
                    // Write successful, clear OP_WRITE.
                    selectionKey.interestOps(SelectionKey.OP_READ);
                }
                lastWriteTime = System.currentTimeMillis();
            }
        }

        private boolean writeToSocket () throws IOException {
            SocketChannel socketChannel = this.socketChannel;
            if (socketChannel == null) throw new SocketException("Connection is closed.");

            ByteBuffer buffer = writeBuffer;
            buffer.flip();
            while (buffer.hasRemaining()) {
                /*
                if (bufferPositionFix) {
                    buffer.compact();
                    buffer.flip();
                }
                 */
                if (socketChannel.write(buffer) == 0) break;
            }
            buffer.compact();

            return buffer.position() == 0;
        }

        public int getLengthLength () {
            return 4;
        }

        public int readLength(ByteBuffer buffer) {
            return buffer.getInt();
        }

        public Object readObject() throws IOException {
            if (socketChannel == null) throw new SocketException("Connection is closed.");

            if (currentObjectLength == 0) {
                // Read the length of the next object from the socket.
                int lengthLength = getLengthLength();
                if (readBuffer.remaining() < lengthLength) {
                    readBuffer.compact();
                    int bytesRead = socketChannel.read(readBuffer);
                    readBuffer.flip();
                    if (bytesRead == -1) throw new SocketException("Connection is closed.");
                    lastReadTime = System.currentTimeMillis();

                    if (readBuffer.remaining() < lengthLength) return null;
                }
                currentObjectLength = readLength(readBuffer);

                if (currentObjectLength <= 0) throw new SocketException("Invalid object length: " + currentObjectLength);
                if (currentObjectLength > readBuffer.capacity())
                    throw new SocketException("Unable to read object larger than read buffer: " + currentObjectLength);
            }

            int length = currentObjectLength;
            if (readBuffer.remaining() < length) {
                // Fill the tcpInputStream.
                readBuffer.compact();
                int bytesRead = socketChannel.read(readBuffer);
                readBuffer.flip();
                if (bytesRead == -1) throw new SocketException("Connection is closed.");
                lastReadTime = System.currentTimeMillis();

                if (readBuffer.remaining() < length) return null;
            }
            currentObjectLength = 0;

            int startPosition = readBuffer.position();
            int oldLimit = readBuffer.limit();
            readBuffer.limit(startPosition + length);
            Object object;
            try {
                // object = read(connection, readBuffer);
                input.setBuffer(readBuffer);
                object = kryo.readClassAndObject(input);
            } catch (Exception ex) {
                throw new IOException("Error during deserialization.", ex);
            }

            readBuffer.limit(oldLimit);
            if (readBuffer.position() - startPosition != length)
                throw new IOException("Incorrect number of bytes (" + (startPosition + length - readBuffer.position())
                    + " remaining) used to deserialize object: " + object);

            return object;
        }

        @Override
        public void close() throws IOException {
            this.socketChannel.close();
        }
    }

    public static class Server implements Runnable, Closeable {
        public final String name;
        public final int port;
        public final org.slf4j.Logger log;
        public final Selector selector;
        public final int writeBufferSize;
        public final int objectBufferSize;
        public final ServerSocketChannel serverChannel;
        public boolean shutdown;
        public final int timeout;
        public int nextConnectionID;
        public final List<Connection> connections;
        public final Kryo kryo;

        public Server(String name, int port) throws IOException {
            this(name, port, 653457, 2048, 250, new Kryo());
        }
        public Server(String name, int port, int writeBufferSize, int objectBufferSize, int timeout, Kryo kryo) throws IOException {
            this.name = name;
            this.port = port;
            this.log = LoggerFactory.getLogger(name);
            this.writeBufferSize = writeBufferSize;
            this.objectBufferSize = objectBufferSize;
            if (timeout <= 0) throw new IllegalArgumentException("Timeout should be > 0");
            this.timeout = timeout;
            // this.discoveryHandler = ServerDiscoveryHandler.DEFAULT;
            this.selector = Selector.open();
            InetSocketAddress tcpPort = new InetSocketAddress(this.port);
            // InetSocketAddress udpPort = new InetSocketAddress(this.port);
            try {
                this.selector.wakeup();
                this.serverChannel = selector.provider().openServerSocketChannel();
                this.serverChannel.socket().bind(tcpPort);
                this.serverChannel.configureBlocking(false);
                this.serverChannel.register(selector, SelectionKey.OP_ACCEPT);
                this.log.debug("Accepting connections on port: " + tcpPort + "/TCP");
                // udp = new UdpConnection(serialization, objectBufferSize);
                // udp.bind(selector, udpPort);
                // log.debug("Accepting connections on port: " + udpPort + "/UDP");
            } catch (IOException ex) {
                close();
                throw ex;
            }
            this.nextConnectionID = 0;
            this.kryo = kryo;
            this.connections = new ArrayList<>(20);

            this.log.info("Server opened.");
        }

        @Override
        public void run() {
            this.shutdown = false;
            while (!this.shutdown) {
                try {
                    // update(250);
                    long startTime = System.currentTimeMillis();
                    int select = selector.select(timeout);
                    if (select == 0) {
                        // NIO freaks and returns immediately with 0 sometimes, so try to keep from hogging the CPU.
                        // nothing to do, so sleep
                        long elapsedTime = System.currentTimeMillis() - startTime;
                        try {
                            if (elapsedTime < 25) Thread.sleep(25 - elapsedTime);
                        } catch (InterruptedException ignored) {
                        }
                        continue;
                    }
                    Set<SelectionKey> keys = selector.selectedKeys();
                    // UdpConnection udp = this.udp;
                    for (Iterator<SelectionKey> iter = keys.iterator(); iter.hasNext();) {
                            /*
                            keepAlive();
                            long time = System.currentTimeMillis();
                            Connection[] connections = this.connections;
                            for (int i = 0, n = connections.length; i < n; i++) {
                                Connection connection = connections[i];
                                if (connection.tcp.needsKeepAlive(time)) connection.sendTCP(FrameworkMessage.keepAlive);
                            }
                             */
                        SelectionKey selectionKey = iter.next();
                        iter.remove();
                        Connection fromConnection = (Connection)selectionKey.attachment();
                        int ops = selectionKey.readyOps();
                        try {
                            if (serverChannel != null && selectionKey.isAcceptable()) {
                                try {
                                    connections.add(new Connection(
                                        writeBufferSize, objectBufferSize, this, serverChannel.accept(),
                                        selector, nextConnectionID++, kryo
                                    ));
                                    if (nextConnectionID == -1) nextConnectionID = 1;
                                } catch (IOException ex) {
                                    log.error("Unable to accept new connection.", ex);
                                }
                            }
                            if (fromConnection != null && selectionKey.isReadable()) {
                                try {
                                    Object read = fromConnection.readObject();
                                    while (read != null) {
                                        read = fromConnection.readObject();
                                    }
                                } catch (IOException ex) {
                                    log.error("Failed to read connection " + fromConnection, ex);
                                    fromConnection.close();
                                }
                            }
                            if (fromConnection != null && selectionKey.isWritable()) {
                                try {
                                    fromConnection.writeToSocket();
                                } catch (IOException ex) {
                                    log.error("Failed to write connection " + fromConnection, ex);
                                    fromConnection.close();
                                }
                            }
                        } catch (CancelledKeyException ex) {
                            if (fromConnection != null)
                                fromConnection.close();
                            else
                                selectionKey.channel().close();
                        }
                    }
                } catch (IOException ex) {
                    this.log.error("Error updating server connections.", ex);
                    try {
                        close();
                    } catch (IOException e) {
                        this.log.error("Error updating server connections.", ex);
                    }
                }
            }
        }

        @Override
        public void close() throws IOException {
            selector.close();
            if (this.serverChannel != null && this.serverChannel.isOpen()) {
                this.serverChannel.close();
            }
        }
    }

}
