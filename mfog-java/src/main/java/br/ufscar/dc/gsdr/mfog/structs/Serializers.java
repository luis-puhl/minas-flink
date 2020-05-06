package br.ufscar.dc.gsdr.mfog.structs;

import com.esotericsoftware.kryo.Kryo;
import com.esotericsoftware.kryo.Serializer;
import com.esotericsoftware.kryo.io.Input;
import com.esotericsoftware.kryo.io.Output;

public class Serializers {
    static public class ClusterSerializer extends Serializer<Cluster> {
        @Override
        public void write(Kryo kryo, Output output, Cluster object) {
            output.writeLong(object.id);
            output.writeFloat(object.variance);
            output.writeString(object.label);
            output.writeString(object.category);
            output.writeLong(object.matches);
            output.writeLong(object.time);
            new PointSerializer().write(kryo, output, object.center);
        }

        @Override
        public Cluster read(Kryo kryo, Input input, Class<Cluster> type) {
            Cluster cluster = new Cluster();
            cluster.id = input.readLong();
            cluster.variance = input.readFloat();
            cluster.label = input.readString();
            cluster.category = input.readString();
            cluster.matches = input.readLong();
            cluster.time = input.readLong();
            cluster.center = new PointSerializer().read(kryo, input, Point.class);
            return cluster;
        }
    }

    static public class PointSerializer extends Serializer<Point> {
        @Override
        public void write(Kryo kryo, Output output, Point object) {
            output.writeLong(object.id);
            output.writeInt(object.value.length);
            output.writeFloats(object.value);
            output.writeLong(object.time);
        }

        @Override
        public Point read(Kryo kryo, Input input, Class<Point> type) {
            Point object = new Point();
            object.id = input.readLong();
            int dimensions = input.readInt();
            object.value = input.readFloats(dimensions);
            object.time = input.readLong();
            return object;
        }
    }

    static public class LabeledExampleSerializer extends Serializer<LabeledExample>  {
        @Override
        public void write(Kryo kryo, Output output, LabeledExample object) {
            output.writeString(object.label);
            new PointSerializer().write(kryo, output, object.point);
        }

        @Override
        public LabeledExample read(Kryo kryo, Input input, Class<LabeledExample> type) {
            LabeledExample labeledExample = new LabeledExample();
            labeledExample.label = input.readString();
            labeledExample.point = new PointSerializer().read(kryo, input, Point.class);
            return labeledExample;
        }
    }

    static public class ModelSerializer extends Serializer<Model> {
        @Override
        public void write(Kryo kryo, Output output, Model object) {
            output.writeInt(object.model.size());
            ClusterSerializer clusterSerializer = new ClusterSerializer();
            for (Cluster cluster : object.model) {
                clusterSerializer.write(kryo, output, cluster);
            }
        }

        @Override
        public Model read(Kryo kryo, Input input, Class<Model> type) {
            Model model = new Model();
            int modelSize = input.readInt();
            ClusterSerializer clusterSerializer = new ClusterSerializer();
            for (int i = 0; i < modelSize; i++) {
                model.model.add(clusterSerializer.read(kryo, input, Cluster.class));
            }
            return model;
        }
    }

    static public Kryo getKryo() {
        Kryo kryo = new Kryo();
        kryo.register(Cluster.class, new ClusterSerializer());
        kryo.register(Point.class, new PointSerializer());
        kryo.register(LabeledExample.class, new LabeledExampleSerializer());
        kryo.register(Model.class, new ModelSerializer());
        kryo.setRegistrationRequired(true);
        return kryo;
    }

    static public void main(String[] args) throws Exception {
        Kryo kryo = getKryo();
        Cluster a = new Cluster();
        a.label = "Hello Kryo!";
        a.center = Point.zero(22);
        System.out.println(a);

        Output output = new Output(1024, -1);
        kryo.writeObject(output, a);

        Input input = new Input(output.getBuffer(), 0, output.position());
        Cluster b = kryo.readObject(input, Cluster.class);
        input.close();
        System.out.println(b);

        if (!b.equals(a)) {
            throw new AssertionError("Serialization round trip failed, cluster differ.");
        }
    }

}
