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

import br.ufscar.dc.gsdr.mfog.structs.Message;
import br.ufscar.dc.gsdr.mfog.structs.SelfDataStreamSerializable;
import br.ufscar.dc.gsdr.mfog.structs.Serializers;
import br.ufscar.dc.gsdr.mfog.util.MfogManager;
import br.ufscar.dc.gsdr.mfog.util.TimeIt;
import com.esotericsoftware.kryonet.Client;
import com.esotericsoftware.kryonet.Connection;
import com.esotericsoftware.kryonet.Listener;
import org.apache.flink.streaming.api.functions.source.SourceFunction;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;

import static org.apache.flink.util.NetUtils.isValidClientPort;
import static org.apache.flink.util.Preconditions.checkArgument;
import static org.apache.flink.util.Preconditions.checkNotNull;

public class SocketGenericSource<T extends SelfDataStreamSerializable<T>> implements SourceFunction<T> {
    protected static final long serialVersionUID = 1L;
    public static int DEFAULT_DELAY_BETWEEN_RETRIES = 1000;
    public static int DEFAULT_MAX_RETRIES = 10;
    public static int DEFAULT_TIMEOUT = 10;
    protected final String hostname;
    protected final int port;
    protected final long maxNumRetries;
    protected final long delayBetweenRetries;
    protected final int connectionTimeout;
    protected final Class<T> typeInfo;
    protected final String sourceName;
    protected transient Logger log;
    protected volatile boolean isRunning = true;
    Client client;
    long i = 0;

    public SocketGenericSource(String hostname, int port, Class<T> typeInfo, String sourceName) {
        this(hostname, port, DEFAULT_MAX_RETRIES, DEFAULT_DELAY_BETWEEN_RETRIES, DEFAULT_TIMEOUT, typeInfo, sourceName);
    }

    public SocketGenericSource(
        String hostname, int port, long maxNumRetries, long delayBetweenRetries, int connectionTimeout, Class<T> typeInfo, String sourceName
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
        this.connectionTimeout = connectionTimeout;
        this.getLog().info("constructor {}", port);
    }

    Logger getLog() {
        if (log == null) {
            this.log = LoggerFactory.getLogger(
                br.ufscar.dc.gsdr.mfog.util.Logger.getLogger(SocketGenericSource.class, typeInfo).getServiceName());
        }
        return this.log;
    }

    @Override
    public void run(SourceContext<T> ctx) throws Exception {
        long attempt = 0;
        getLog();
        Client client = new Client();
        Serializers.registerMfogStructs(client.getKryo());
        client.addListener(new Listener() {
            @Override
            public void received(Connection connection, Object message) {
                super.received(connection, message);
                if (message instanceof Message) {
                    Message msg = (Message) message;
                    if (msg.isDone()) {
                        client.close();
                    }
                } else if (typeInfo.isInstance(message)) {
                    i++;
                    ctx.collect((T) message);
                }
            }
        });
        // client.start();
        log.info("Connecting to server socket {}:{}", hostname, port);
        client.connect(5000, MfogManager.SERVICES_HOSTNAME, MfogManager.MODEL_STORE_PORT);
        client.sendTCP(new Message(Message.Intentions.SEND_ONLY));
        this.client = client;
        //
        TimeIt timeIt = new TimeIt().start();
        while (isRunning) {
            try {
                while (isRunning) {
                    client.update(connectionTimeout);
                }
            } catch (IOException e) {
                log.warn(e.getMessage());
            }
            if (isRunning) {
                attempt++;
                if (maxNumRetries == -1 || attempt < maxNumRetries) {
                    log.warn("Lost connection to server socket. Retrying in " + delayBetweenRetries + " msecs...");
                    Thread.sleep(delayBetweenRetries);
                    client.reconnect();
                } else {
                    // this should probably be here, but some examples expect simple exists of the stream source
                    // throw new EOFException("Reached end of stream and reconnects are not enabled.");
                    break;
                }
            }
        }
        client.dispose();
        log.info(timeIt.finish(i));
    }

    @Override
    public void cancel() {
        isRunning = false;

        // we need to close the socket as well, because the Thread.interrupt() function will
        // not wake the thread in the socketStream.read() method when blocked.
        Client theSocket = this.client;
        if (theSocket != null) {
            try {
                theSocket.dispose();
            } catch (IOException e) {
                log.error(e.getMessage());
            }
        }
    }
}
