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

import java.io.IOException;

import br.ufscar.dc.gsdr.mfog.structs.WithSerializable;
import br.ufscar.dc.gsdr.mfog.util.ServerClient;

import org.apache.flink.streaming.api.functions.source.SourceFunction;
import static org.apache.flink.util.NetUtils.isValidClientPort;
import static org.apache.flink.util.Preconditions.checkArgument;
import static org.apache.flink.util.Preconditions.checkNotNull;

@Deprecated
public class SocketStreamFunction<T extends WithSerializable<T>> implements SourceFunction<T> {

    private static final long serialVersionUID = 1L;

    // private static final br.ufscar.dc.gsdr.mfog.util.Logger LOG = br.ufscar.dc.gsdr.mfog.util.Logger.getLogger(SocketStreamFunction.class);
    private static final org.slf4j.Logger LOG = org.slf4j.LoggerFactory.getLogger(SocketStreamFunction.class);

    /** Default delay between successive connection attempts. */
    private static final int DEFAULT_CONNECTION_RETRY_SLEEP = 500;

    /** Default connection timeout when connecting to the server socket (infinite). */
    private static final int CONNECTION_TIMEOUT_TIME = 0;

    private final String hostname;
    private final int port;
    private final int maxNumRetries;
    private final long delayBetweenRetries;

    private transient ServerClient<T> currentSocket;

    private volatile boolean isRunning = true;
    private Class<T> typeParameterClass;
    private T reusableObject;
    private boolean withGzip;

    public SocketStreamFunction(String hostname, int port, int maxNumRetries, long delayBetweenRetries, Class<T> typeParameterClass, T reusableObject, boolean withGzip) {
        checkArgument(isValidClientPort(port), "port is out of range");
        checkArgument(maxNumRetries >= -1, "maxNumRetries must be zero or larger (num retries), or -1 (infinite retries)");
        checkArgument(delayBetweenRetries >= 0, "delayBetweenRetries must be zero or positive");

        this.hostname = checkNotNull(hostname, "hostname must not be null");
        this.port = port;
        this.maxNumRetries = maxNumRetries;
        this.delayBetweenRetries = delayBetweenRetries;
        this.typeParameterClass = typeParameterClass;
        this.reusableObject = reusableObject;
        this.withGzip = withGzip;
    }

    @Override
    public void run(SourceContext<T> ctx) throws Exception {
        long attempt = 0;
        LOG.info("SocketStreamFunction attempt " + attempt + ':' + isRunning);
        currentSocket = new ServerClient<>(typeParameterClass, reusableObject, withGzip, SocketStreamFunction.class);
        while (isRunning) {
            LOG.info("Connecting to server socket " + hostname + ':' + port);
            currentSocket.client(hostname, port, maxNumRetries, delayBetweenRetries);
            if (!currentSocket.isConnected() && isRunning) {
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
            while (isRunning && currentSocket.hasNext()) {
                T next = currentSocket.next();
                if (next != null) {
                    ctx.collect(next);
                } else {
                    break;
                }
            }
            currentSocket.close();
        }
    }

    @Override
    public void cancel() {
        isRunning = false;
        try {
            this.currentSocket.close();
        } catch (IOException e) {
            LOG.error(e.toString());
        }
        // we need to close the socket as well, because the Thread.interrupt() function will
        // not wake the thread in the socketStream.read() method when blocked.
    }
}