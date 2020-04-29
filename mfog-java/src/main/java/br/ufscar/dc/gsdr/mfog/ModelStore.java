package br.ufscar.dc.gsdr.mfog;

import br.ufscar.dc.gsdr.mfog.util.Logger;
import br.ufscar.dc.gsdr.mfog.util.MfogManager;
import br.ufscar.dc.gsdr.mfog.util.TcpUtil;

import java.util.ArrayList;
import java.util.List;

public class ModelStore {
    static List<String> model = new ArrayList<>(100);

    public static void main(String[] args) {
        Logger LOG = Logger.getLogger("ModelStore");
        TcpUtil<String> server = new TcpUtil<>(
                "ModelStore",
                MfogManager.MODEL_STORE_PORT, () -> {
                    LOG.info("current model size =" + model.size());
                    return model.iterator();
                },
                (s) -> {
                    model.stream().limit(1).forEach((i) -> {
                        if (i.equals(s)) LOG.info("current model size =" + model.size());
                    });
                    return s;
                },
                model
        );
        server.server(2);
        LOG.info("done");
    }
}
