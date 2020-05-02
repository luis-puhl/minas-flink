/**
* Copyright 2020 Luis Puhl
* 
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
* 
*     http://www.apache.org/licenses/LICENSE-2.0
* 
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

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
