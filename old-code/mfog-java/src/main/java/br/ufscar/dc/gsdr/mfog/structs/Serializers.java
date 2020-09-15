package br.ufscar.dc.gsdr.mfog.structs;

import com.esotericsoftware.kryo.Kryo;
import com.esotericsoftware.kryo.io.Input;
import com.esotericsoftware.kryo.io.Output;

public class Serializers {

    static Kryo kryo;

    static public Kryo getKryo() {
        if (kryo != null) {
            return kryo;
        }
        kryo = new Kryo();
        kryo.register(Cluster.class, new Cluster());
        kryo.register(Point.class, new Point());
        kryo.register(LabeledExample.class, new LabeledExample());
        kryo.register(Model.class, new Model());
        kryo.setRegistrationRequired(true);
        return kryo;
    }

    static public Kryo registerMfogStructs(Kryo kryo) {
        kryo.register(Cluster.class, new Cluster());
        kryo.register(Point.class, new Point());
        kryo.register(LabeledExample.class, new LabeledExample());
        kryo.register(Model.class, new Model());
        kryo.register(Message.class);
        kryo.register(Message.Intentions.class);
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
