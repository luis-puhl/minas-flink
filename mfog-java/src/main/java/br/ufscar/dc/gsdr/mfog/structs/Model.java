package br.ufscar.dc.gsdr.mfog.structs;

import java.io.Serializable;
import java.util.ArrayList;
import java.util.List;

public class Model implements Serializable {
    public List<Cluster> model = new ArrayList<>(100);
    public int size = 0;

    public int size() {
        return size;
    }
    public boolean isEmpty() {
        return this.model.isEmpty();
    }

    public LabeledExample classify(Point example) {
        String minLabel = "no model";
        double minDist = Double.MAX_VALUE;
        for (Cluster cluster : this.model) {
            double distance = cluster.center.distance(example);
            if (distance < minDist) {
                minLabel = cluster.label;
                minDist = distance;
            }
        }
        return new LabeledExample(minLabel, example);
    }

    @Override
    public String toString() {
        return "Model{model=" + model.size() +"}";
    }
}
