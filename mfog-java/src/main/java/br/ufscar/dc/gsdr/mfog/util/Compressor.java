package br.ufscar.dc.gsdr.mfog.util;

import java.io.*;
import java.nio.ByteBuffer;
import java.util.zip.GZIPInputStream;
import java.util.zip.GZIPOutputStream;

public class Compressor {
    static class Encoder {
        PipedInputStream pipeIn;
        PipedOutputStream pipeOut;
        ObjectOutputStream objectOutputStream;
        Logger log = Logger.getLogger(Encoder.class);
        Encoder() throws IOException {
            log.info("new PipedInputStream");
            pipeIn = new PipedInputStream();
            log.info("new PipedOutputStream");
            pipeOut = new PipedOutputStream(pipeIn);
            log.info("new GZIPOutputStream");
            GZIPOutputStream gzipOutputStream = new GZIPOutputStream(pipeOut);
            log.info("new ObjectOutputStream");
            objectOutputStream = new ObjectOutputStream(gzipOutputStream);
        }
        public ByteBuffer encode(Serializable object, ByteBuffer byteBuffer) throws IOException {
            objectOutputStream.writeObject(object);
            int available = pipeIn.available();
            if (available > byteBuffer.remaining()) {
                byteBuffer.limit(byteBuffer.limit() + available);
            }
            byte[] buffer = byteBuffer.array();
            int read = pipeIn.read(buffer);
            return byteBuffer;
        }
    }
    static class Decoder {
        PipedInputStream pipeIn;
        PipedOutputStream pipeOut;
        ObjectInputStream objectInputStream;
        Logger log = Logger.getLogger(Decoder.class);
        Decoder() throws IOException {
            log.info("new PipedOutputStream");
            pipeOut = new PipedOutputStream();
            log.info("new PipedInputStream");
            pipeIn = new PipedInputStream(pipeOut);
            log.info("new ObjectInputStream");
            objectInputStream = new ObjectInputStream(new GZIPInputStream(pipeIn));
        }
        public Object decode(ByteBuffer byteBuffer) throws IOException, ClassNotFoundException {
            pipeOut.write(byteBuffer.array());
            return objectInputStream.readObject();
        }
    }

    ByteBuffer byteBuffer;
    Encoder encoder;
    Decoder decoder;
    Logger log = Logger.getLogger(Decoder.class);
    public Compressor(int bufferSize) throws IOException {
        log.info("new Encoder");
        encoder = new Encoder();
        log.info("new Decoder");
        decoder = new Decoder();
        log.info("ByteBuffer.allocate");
        byteBuffer = ByteBuffer.allocate(bufferSize);
        log.info("Compressor initialized");
    }
    public byte[] encode(Serializable object) throws IOException {
        byteBuffer.clear();
        encoder.encode(object, byteBuffer);
        return byteBuffer.array();
    }
    public Object decode() throws IOException, ClassNotFoundException {
        Object decoded = decoder.decode(byteBuffer);
        byteBuffer.clear();
        return decoded;
    }
}
