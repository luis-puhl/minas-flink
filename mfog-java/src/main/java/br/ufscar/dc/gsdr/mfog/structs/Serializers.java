package br.ufscar.dc.gsdr.mfog.structs;

import com.esotericsoftware.kryo.Kryo;
import com.esotericsoftware.kryo.Serializer;
import com.esotericsoftware.kryo.io.Input;
import com.esotericsoftware.kryo.io.Output;

public class Serializers {
    static public class ClusterSerializer extends Serializer<Cluster> {
        @Override
        public void write(Kryo kryo, Output output, Cluster object) {
            object.toDataOutputStream(output);
        }

        @Override
        public Cluster read(Kryo kryo, Input input, Class<Cluster> type) {
            return new Cluster().reuseFromDataInputStream(input);
        }
    }
    static public class PointSerializer extends Serializer<Point> {
        @Override
        public void write(Kryo kryo, Output output, Point object) {
            object.toDataOutputStream(output);
        }

        @Override
        public Point read(Kryo kryo, Input input, Class<Point> type) {
            return new Point().reuseFromDataInputStream(input);
        }
    }
    static public class LabeledExampleSerializer extends Serializer<LabeledExample> {
        @Override
        public void write(Kryo kryo, Output output, LabeledExample object) {
            object.toDataOutputStream(output);
        }

        @Override
        public LabeledExample read(Kryo kryo, Input input, Class<LabeledExample> type) {
            return new LabeledExample().reuseFromDataInputStream(input);
        }
    }
}
