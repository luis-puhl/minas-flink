package br.ufscar.dc.gsdr.mfog.structs;

import com.esotericsoftware.kryo.io.Input;
import com.esotericsoftware.kryo.io.Output;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;

public interface SelfDataStreamSerializable<T> {
    T read(DataInputStream in, T reuse) throws IOException;
    void write(DataOutputStream out, T toWrite) throws IOException;

    // T read(Input in);
    // void write(Output out);
}