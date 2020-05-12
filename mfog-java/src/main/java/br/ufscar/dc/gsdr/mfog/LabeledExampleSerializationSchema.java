package br.ufscar.dc.gsdr.mfog;

import br.ufscar.dc.gsdr.mfog.structs.LabeledExample;
import org.apache.flink.api.common.serialization.SerializationSchema;
import org.slf4j.LoggerFactory;

import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.Serializable;

public class LabeledExampleSerializationSchema implements SerializationSchema<LabeledExample>, Serializable {
    static final org.slf4j.Logger log = LoggerFactory.getLogger(LabeledExampleSerializationSchema.class);
    protected transient ByteArrayOutputStream byteArrayOutputStream;
    protected transient DataOutputStream dataOutputStream;

    @Override
    public byte[] serialize(LabeledExample element) {
        if (byteArrayOutputStream == null) byteArrayOutputStream = new ByteArrayOutputStream(1024);
        if (dataOutputStream == null) dataOutputStream = new DataOutputStream(byteArrayOutputStream);
        try {
            element.write(dataOutputStream, element);
            dataOutputStream.flush();
        } catch (IOException e) {
            log.error("Failed to Flush", e);
        }
        byte[] bytes = byteArrayOutputStream.toByteArray();
        byteArrayOutputStream.reset();
        return bytes;
    }
}
