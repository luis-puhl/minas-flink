/**
 * Copyright 2020 Luis Puhl
 * <p>
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * <p>
 * http://www.apache.org/licenses/LICENSE-2.0
 * <p>
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package br.ufscar.dc.gsdr.mfog;

import br.ufscar.dc.gsdr.mfog.structs.Cluster;
import br.ufscar.dc.gsdr.mfog.structs.Model;
import org.apache.flink.api.common.functions.RichMapFunction;
import org.apache.flink.api.common.state.ValueState;
import org.apache.flink.api.common.state.ValueStateDescriptor;
import org.apache.flink.configuration.Configuration;
import org.slf4j.LoggerFactory;

import java.util.ArrayList;

public class ModelAggregate extends RichMapFunction<Cluster, Model> {
    final org.slf4j.Logger log = LoggerFactory.getLogger(ModelAggregate.class);
    ValueState<Model> modelState;

    @Override
    public void open(Configuration parameters) throws Exception {
        super.open(parameters);
        modelState = getRuntimeContext().getState(new ValueStateDescriptor<Model>("model", Model.class));
        log.info("Map2Model Open");
    }

    @Override
    public Model map(Cluster value) throws Exception {
        Model model = modelState.value();
        if (model == null) {
            model = new Model();
            model.model = new ArrayList<>(100);
        }
        model.model.add(value);
        modelState.update(model);
        return model;
    }
}
