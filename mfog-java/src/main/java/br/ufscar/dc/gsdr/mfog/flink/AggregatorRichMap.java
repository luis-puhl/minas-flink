package br.ufscar.dc.gsdr.mfog.flink;

import br.ufscar.dc.gsdr.mfog.structs.Cluster;
import org.apache.flink.api.common.functions.RichMapFunction;
import org.apache.flink.configuration.Configuration;

import java.util.ArrayList;
import java.util.List;

public class AggregatorRichMap<T> extends RichMapFunction<T, List<T>> {
    List<T> model;

    @Override
    public List<T> map(T value) {
        model.add(value);
        return model;
    }

    @Override
    public void open(Configuration parameters) throws Exception {
        super.open(parameters);
        model = new ArrayList<>(100);
    }
}
