package br.ufscar.dc.gsdr.mfog.structs;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;

public interface WithSerializable<T> {
    T reuseFromDataInputStream(DataInputStream in) throws IOException;
    void toDataOutputStream(DataOutputStream out) throws IOException;
}