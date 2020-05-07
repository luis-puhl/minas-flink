package br.ufscar.dc.gsdr.mfog.structs;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;

public interface SelfDataStreamSerializable<T> {
    T read(DataInputStream in, T reuse) throws IOException;
    void write(DataOutputStream out, T toWrite) throws IOException;
}