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

    static public class ModelSerializer extends Serializer<Model> {
        @Override
        public void write(Kryo kryo, Output output, Model object) {
            output.writeInt(object.model.size());
            for (Cluster cluster : object.model) {
                cluster.toDataOutputStream(output);
            }
        }

        @Override
        public Model read(Kryo kryo, Input input, Class<Model> type) {
            Model model = new Model();
            model.size = input.readInt();
            for (int i = 0; i < model.size; i++) {
                model.model.add(new Cluster().reuseFromDataInputStream(input));
            }
            return model;
        }
    }

}
