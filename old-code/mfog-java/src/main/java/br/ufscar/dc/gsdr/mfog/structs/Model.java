package br.ufscar.dc.gsdr.mfog.structs;

import com.esotericsoftware.kryo.Kryo;
import com.esotericsoftware.kryo.Serializer;
import com.esotericsoftware.kryo.io.Input;
import com.esotericsoftware.kryo.io.Output;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.Serializable;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class Model extends Serializer<Model> implements Serializable, SelfDataStreamSerializable<Model> {
    public List<Cluster> model = new ArrayList<>(100);
    public List<Cluster> sleep = new ArrayList<>(100);
    public Map<Long, Long> times = new HashMap<>();
    public long matches = 0;
    public long lastSleepTest = 0;

    public LabeledExample classify(Point example) {
        String minLabel = "no model";
        long minId = -1;
        double minDist = Double.MAX_VALUE;
        for (Cluster cluster : this.model) {
            double distance = cluster.center.distance(example);
            if (distance < minDist) {
                minId = cluster.id;
                minLabel = cluster.label;
                minDist = distance;
            }
        }
        matches++;
        if (matches == Long.MAX_VALUE) {
            // looped
            for (Map.Entry<Long, Long> entry : times.entrySet()) {
                entry.setValue(-(entry.getValue() - Long.MAX_VALUE));
            }
            matches = 0;
            lastSleepTest = -(lastSleepTest - Long.MAX_VALUE);
        }
        times.put(minId, matches);
        return new LabeledExample(minLabel, example);
    }

    /**
     * Minas uses thresholdForgettingPast = 10000
     * But does not specify minimum model size, but we know 6 is a CluStream generated model size,
     * so 5 is good enough.
     */
    public void putToSleep() {
        this.putToSleep(10000, 5);
    }

    public void putToSleep(long diff, int minModelSize) {
        if (matches - lastSleepTest < diff) {
            return;
        }
        lastSleepTest = matches;
        List<Cluster> toRemove = new ArrayList<>(model.size());
        for (Map.Entry<Long, Long> entry : times.entrySet()) {
            long id = entry.getKey();
            long lastMatch = entry.getValue();
            if (matches - lastMatch > diff) {
                for (Cluster cluster : model) {
                    if (cluster.id == id) {
                        toRemove.add(cluster);
                        break;
                    }
                }
            }
        }
        if (model.size() - toRemove.size() > minModelSize) {
            sleep.addAll(toRemove);
            model.removeAll(toRemove);
        }
    }

    @Override
    public String toString() {
        return "Model{model=" + model.size() + ", sleep=" + sleep.size() + "}";
    }


    @Override
    public void write(Kryo kryo, Output output, Model object) {
        output.writeInt(object.model.size());
        for (Cluster cluster : object.model) {
            cluster.write(kryo, output, cluster);
        }
    }

    @Override
    public Model read(Kryo kryo, Input input, Class<Model> type) {
        Model model = new Model();
        int modelSize = input.readInt();
        Cluster serializer = new Cluster();
        for (int i = 0; i < modelSize; i++) {
            model.model.add(serializer.read(kryo, input, Cluster.class));
        }
        return model;
    }

    public Model read(DataInputStream in, Model model) throws IOException {
        int modelSize = in.readInt();
        int interSectSize = Math.min(model.model.size(), modelSize);
        int i = 0;
        for (; i < interSectSize; i++) {
            // reuse
            Cluster cluster = model.model.get(i);
            cluster.read(in, cluster);
        }
        Cluster serializer = new Cluster();
        for (; i < modelSize; i++) {
            model.model.add(serializer.read(in, new Cluster()));
        }
        if (interSectSize > modelSize) {
            while (model.model.size() > modelSize) {
                model.model.remove(model.model.size() - 1);
            }
        }
        return model;
    }

    public void write(DataOutputStream out, Model model) throws IOException {
        out.writeInt(model.model.size());
        for (Cluster cluster : model.model) {
            cluster.write(out, cluster);
        }
    }
}
