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
import org.apache.flink.streaming.api.functions.source.SourceFunction;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.BufferedInputStream;
import java.io.DataInputStream;
import java.io.IOException;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.util.zip.GZIPInputStream;

import static org.apache.flink.util.NetUtils.isValidClientPort;
import static org.apache.flink.util.Preconditions.checkArgument;
import static org.apache.flink.util.Preconditions.checkNotNull;

public class SocketGenericSource<T extends WithSerializable<T>> implements SourceFunction<T> {
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
    protected final Class<T> typeInfo;
    protected final String sourceName;
    protected final boolean withGzip;
    protected transient Logger log;
    protected transient Socket currentSocket;
    protected transient DataInputStream reader;
    protected volatile boolean isRunning = true;
    protected T reusableObject;

    public SocketGenericSource(String hostname, int port, T reusableObject, Class<T> typeInfo, String sourceName) {
        this(
            hostname, port, DEFAULT_MAX_RETRIES, DEFAULT_DELAY_BETWEEN_RETRIES, DEFAULT_TIMEOUT, reusableObject,
            typeInfo, sourceName, DEFAULT_GZIP
        );
    }

    public SocketGenericSource(
        String hostname, int port, long maxNumRetries, long delayBetweenRetries, int connectionTimeout, T reusableObject, Class<T> typeInfo, String sourceName, boolean withGzip
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
        this.getLog().info("constructor {}", port);
    }

    Logger getLog() {
        if (log == null) {
            this.log = LoggerFactory.getLogger(
                br.ufscar.dc.gsdr.mfog.util.Logger.getLoggerMame(SocketGenericSource.class, typeInfo));
        }
        return this.log;
    }

    @Override
    public void run(SourceContext<T> ctx) throws Exception {
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
                            ctx.collect(reusableObject);
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
    public void cancel() {
        isRunning = false;

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
}
