/**
* Copyright 2020 Luis Puhl
* 
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
* 
*     http://www.apache.org/licenses/LICENSE-2.0
* 
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

package br.ufscar.dc.gsdr.mfog.flink;

import org.apache.commons.lang3.SerializationException;
import org.apache.commons.lang3.SerializationUtils;
import org.apache.flink.streaming.api.functions.source.SocketTextStreamFunction;
import org.apache.flink.streaming.api.functions.source.SourceFunction;
import org.apache.flink.util.IOUtils;

import java.io.DataInputStream;
import java.io.InputStream;
import java.net.InetSocketAddress;
import java.net.Socket;

import static org.apache.flink.util.NetUtils.isValidClientPort;
import static org.apache.flink.util.Preconditions.checkArgument;
import static org.apache.flink.util.Preconditions.checkNotNull;

public class SocketStreamFunction<T> implements SourceFunction<T> {

    private static final long serialVersionUID = 1L;

    private static final br.ufscar.dc.gsdr.mfog.util.Logger LOG = br.ufscar.dc.gsdr.mfog.util.Logger.getLogger(SocketStreamFunction.class);
    // private static final org.slf4j.Logger LOG = org.slf4j.LoggerFactory.getLogger(SocketTextStreamFunction.class);

    /** Default delay between successive connection attempts. */
    private static final int DEFAULT_CONNECTION_RETRY_SLEEP = 500;

    /** Default connection timeout when connecting to the server socket (infinite). */
    private static final int CONNECTION_TIMEOUT_TIME = 0;


    private final String hostname;
    private final int port;
    private final long maxNumRetries;
    private final long delayBetweenRetries;

    private transient Socket currentSocket;

    private volatile boolean isRunning = true;

    public SocketStreamFunction(String hostname, int port) {
        this(hostname, port, 0);
    }
    public SocketStreamFunction(String hostname, int port, long maxNumRetries) {
        this(hostname, port, maxNumRetries, DEFAULT_CONNECTION_RETRY_SLEEP);
    }

    public SocketStreamFunction(String hostname, int port, long maxNumRetries, long delayBetweenRetries) {
        checkArgument(isValidClientPort(port), "port is out of range");
        checkArgument(maxNumRetries >= -1, "maxNumRetries must be zero or larger (num retries), or -1 (infinite retries)");
        checkArgument(delayBetweenRetries >= 0, "delayBetweenRetries must be zero or positive");

        this.hostname = checkNotNull(hostname, "hostname must not be null");
        this.port = port;
        this.maxNumRetries = maxNumRetries;
        this.delayBetweenRetries = delayBetweenRetries;
    }

    @Override
    public void run(SourceContext<T> ctx) throws Exception {
        long attempt = 0;
        LOG.info("SocketStreamFunction attempt " + attempt + ':' + isRunning);

        while (isRunning) {

            try (Socket socket = new Socket()) {
                currentSocket = socket;

                LOG.info("Connecting to server socket " + hostname + ':' + port);
                socket.connect(new InetSocketAddress(hostname, port), CONNECTION_TIMEOUT_TIME);
                try (InputStream reader = socket.getInputStream()) {
                    DataInputStream dataInputStream = new DataInputStream(reader);
                    T next;
                    do {
                        next = null;
                        try {
                            int length;
                            try {
                                length = dataInputStream.readInt();
                            } catch (Exception e) {
                                break;
                            }
                            if (length <= 0) continue;
                            byte[] message = new byte[length];
                            dataInputStream.readFully(message, 0, message.length);
                            next = SerializationUtils.deserialize(message);
                            ctx.collect(next);
                        } catch (SerializationException e) {
                            LOG.error(e);
                        }
                    } while (isRunning && next != null);
                }
            }

            // if we dropped out of this loop due to an EOF, sleep and retry
            if (isRunning) {
                attempt++;
                if (maxNumRetries == -1 || attempt < maxNumRetries) {
                    LOG.warn("Lost connection to server socket. Retrying in " + delayBetweenRetries + " msecs...");
                    Thread.sleep(delayBetweenRetries);
                }
                else {
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
            IOUtils.closeSocket(theSocket);
        }
    }
}