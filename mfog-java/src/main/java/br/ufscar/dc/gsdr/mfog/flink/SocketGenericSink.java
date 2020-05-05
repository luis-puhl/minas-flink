/**
 * Copyright 2020 Luis Puhl
 * <p>
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * <p>
 * http://www.apache.org/licenses/LICENSE-2.0
 * <p>
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package br.ufscar.dc.gsdr.mfog.flink;

import br.ufscar.dc.gsdr.mfog.structs.WithSerializable;
import org.apache.flink.api.common.serialization.SerializationSchema;
import org.apache.flink.configuration.Configuration;
import org.apache.flink.streaming.api.functions.sink.RichSinkFunction;
import org.apache.flink.streaming.api.functions.source.SourceFunction;
import org.apache.flink.util.SerializableObject;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.BufferedInputStream;
import java.io.DataInputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.util.zip.GZIPInputStream;

import static org.apache.flink.util.NetUtils.isValidClientPort;
import static org.apache.flink.util.Preconditions.checkArgument;
import static org.apache.flink.util.Preconditions.checkNotNull;

public class SocketGenericSink<IN extends WithSerializable<IN>> extends RichSinkFunction<IN> {
    protected static final long serialVersionUID = 1L;
    public static int DEFAULT_DELAY_BETWEEN_RETRIES = 1000;
    public static int DEFAULT_MAX_RETRIES = 10;
    public static int DEFAULT_TIMEOUT = 10;
    public static boolean DEFAULT_GZIP = false;
    protected final String hostname;
    protected final int port;
    protected final long maxNumRetries;
    protected final long delayBetweenRetries;
    protected final int connectionTimeout;
    protected final Class<IN> typeInfo;
    protected final String sourceName;
    protected final boolean withGzip;
    protected transient Logger log;
    protected transient Socket currentSocket;
    protected transient DataInputStream reader;
    protected volatile boolean isRunning = true;
    protected IN reusableObject;
    //
    private static final int CONNECTION_RETRY_DELAY = 500;
    private final SerializableObject lock = new SerializableObject();
    private final SerializationSchema<IN> schema;
    private final boolean autoFlush;
    private transient Socket client;
    private transient OutputStream outputStream;
    private int retries;

    public SocketGenericSink(
        String hostname, int port, long maxNumRetries, long delayBetweenRetries, int connectionTimeout, IN reusableObject, Class<IN> typeInfo, String sourceName, boolean withGzip, SerializationSchema<IN> schema, boolean autoflush
    ) {
        checkArgument(isValidClientPort(port), "port is out of range");
        checkArgument(
            maxNumRetries >= -1, "maxNumRetries must be zero or larger (num retries), or -1 (infinite retries)");
        checkArgument(delayBetweenRetries >= 0, "delayBetweenRetries must be zero or positive");

        this.hostname = checkNotNull(hostname, "hostname must not be null");
        this.port = port;
        this.maxNumRetries = maxNumRetries;
        this.typeInfo = typeInfo;
        this.sourceName = sourceName;
        this.delayBetweenRetries = delayBetweenRetries;
        this.withGzip = withGzip;
        this.reusableObject = reusableObject;
        this.connectionTimeout = connectionTimeout;
        this.schema = checkNotNull(schema);
        this.autoFlush = autoflush;
        this.getLog().info("constructor {}", port);
    }

    Logger getLog() {
        if (log == null) {
            this.log = LoggerFactory.getLogger(
                br.ufscar.dc.gsdr.mfog.util.Logger.getLoggerMame(SocketGenericSink.class, typeInfo));
        }
        return this.log;
    }

    @Override
    public void open(Configuration parameters) throws Exception {
        try {
            synchronized (lock) {
                createConnection();
            }
        }
        catch (IOException e) {
            throw new IOException("Cannot connect to socket server at " + hostname + ":" + port, e);
        }
    }

    @Override
    public void invoke(IN value, Context ctx) throws Exception {
        byte[] msg = schema.serialize(value);

        try {
            outputStream.write(msg);
            if (autoFlush) {
                outputStream.flush();
            }
        }
        catch (IOException e) {
            // if no re-tries are enable, fail immediately
            if (maxNumRetries == 0) {
                throw new IOException("Failed to send message '" + value + "' to socket server at "
                    + hostname + ":" + port + ". Connection re-tries are not enabled.", e);
            }

            log.error("Failed to send message '" + value + "' to socket server at " + hostname + ":" + port +
                ". Trying to reconnect..." , e);

            // do the retries in locked scope, to guard against concurrent close() calls
            // note that the first re-try comes immediately, without a wait!

            synchronized (lock) {
                IOException lastException = null;
                retries = 0;

                while (isRunning && (maxNumRetries < 0 || retries < maxNumRetries)) {

                    // first, clean up the old resources
                    try {
                        if (outputStream != null) {
                            outputStream.close();
                        }
                    }
                    catch (IOException ee) {
                        log.error("Could not close output stream from failed write attempt", ee);
                    }
                    try {
                        if (client != null) {
                            client.close();
                        }
                    }
                    catch (IOException ee) {
                        log.error("Could not close socket from failed write attempt", ee);
                    }

                    // try again
                    retries++;

                    try {
                        // initialize a new connection
                        createConnection();

                        // re-try the write
                        outputStream.write(msg);

                        // success!
                        return;
                    }
                    catch (IOException ee) {
                        lastException = ee;
                        log.error("Re-connect to socket server and send message failed. Retry time(s): " + retries, ee);
                    }

                    // wait before re-attempting to connect
                    lock.wait(CONNECTION_RETRY_DELAY);
                }

                // throw an exception if the task is still running, otherwise simply leave the method
                if (isRunning) {
                    throw new IOException("Failed to send message '" + value + "' to socket server at "
                        + hostname + ":" + port + ". Failed after " + retries + " retries.", lastException);
                }
            }
        }
        long attempt = 0;
        getLog();
        while (isRunning) {
            try (Socket socket = new Socket()) {
                currentSocket = socket;
                log.info("Connecting to server socket {}:{}", hostname, port);
                try {
                    socket.connect(new InetSocketAddress(hostname, port), connectionTimeout);
                    if (withGzip) {
                        reader = new DataInputStream(
                            new GZIPInputStream(new BufferedInputStream(socket.getInputStream())));
                    } else {
                        reader = new DataInputStream(new BufferedInputStream(socket.getInputStream()));
                    }
                    while (isRunning) {
                        try {
                            reusableObject.reuseFromDataInputStream(reader);
                            // ctx.collect(reusableObject);
                        } catch (java.io.EOFException e) {
                            reader.close();
                            socket.close();
                            isRunning = false;
                            log.warn("EOF.");
                            return;
                        }
                    }
                    reader.close();
                } catch (java.net.ConnectException e) {
                    log.warn(e.getMessage());
                }
            }

            // if we dropped out of this loop due to an EOF, sleep and retry
            if (isRunning) {
                attempt++;
                if (maxNumRetries == -1 || attempt < maxNumRetries) {
                    log.warn("Lost connection to server socket. Retrying in " + delayBetweenRetries + " msecs...");
                    Thread.sleep(delayBetweenRetries);
                } else {
                    // this should probably be here, but some examples expect simple exists of the stream source
                    // throw new EOFException("Reached end of stream and reconnects are not enabled.");
                    break;
                }
            }
        }
    }

    @Override
    public void close() throws Exception {
        // flag this as not running any more
        isRunning = false;

        // clean up in locked scope, so there is no concurrent change to the stream and client
        synchronized (lock) {
            // we notify first (this statement cannot fail). The notified thread will not continue
            // anyways before it can re-acquire the lock
            lock.notifyAll();

            try {
                if (outputStream != null) {
                    outputStream.close();
                }
            }
            finally {
                if (client != null) {
                    client.close();
                }
            }
        }
        // we need to close the socket as well, because the Thread.interrupt() function will
        // not wake the thread in the socketStream.read() method when blocked.
        Socket theSocket = this.currentSocket;
        if (theSocket != null) {
            try {
                theSocket.close();
            } catch (IOException e) {
                log.error(e.getMessage());
            }
        }
    }

    private void createConnection() throws IOException {
        client = new Socket(hostname, port);
        client.setKeepAlive(true);
        client.setTcpNoDelay(true);

        outputStream = client.getOutputStream();
    }

    int getCurrentNumberOfRetries() {
        synchronized (lock) {
            return retries;
        }
    }
}
