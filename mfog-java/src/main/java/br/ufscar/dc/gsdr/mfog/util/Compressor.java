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
        Encoder() throws IOException {
            pipeIn = new PipedInputStream();
            pipeOut = new PipedOutputStream();
            objectOutputStream = new ObjectOutputStream(new GZIPOutputStream(pipeOut));
            pipeOut.connect(pipeIn);
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
        Decoder() throws IOException {
            pipeIn = new PipedInputStream();
            pipeOut = new PipedOutputStream();
            objectInputStream = new ObjectInputStream(new GZIPInputStream(pipeIn));
            pipeIn.connect(pipeOut);
        }
        public Object decode(ByteBuffer byteBuffer) throws IOException, ClassNotFoundException {
            pipeOut.write(byteBuffer.array());
            return objectInputStream.readObject();
        }
    }

    ByteBuffer byteBuffer;
    Encoder encoder;
    Decoder decoder;
    public Compressor(int bufferSize) throws IOException {
        encoder = new Encoder();
        decoder = new Decoder();
        byteBuffer = ByteBuffer.allocate(bufferSize);
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
