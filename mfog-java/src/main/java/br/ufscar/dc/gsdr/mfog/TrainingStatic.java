package br.ufscar.dc.gsdr.mfog;

import br.ufscar.dc.gsdr.mfog.structs.Cluster;
import br.ufscar.dc.gsdr.mfog.util.MfogManager;
import br.ufscar.dc.gsdr.mfog.util.TcpUtil;

import java.io.*;
import java.util.stream.Stream;

public class TrainingStatic {
    public static void main(String[] args) throws IOException {
        String path = "datasets" + File.separator + "models" + File.separator + "offline.csv";
        BufferedReader in = new BufferedReader(new FileReader(path));
        Stream<String> model = in.lines().skip(1).map(Cluster::fromMinasCsv).map(c -> c.json().toString());
        //
        TcpUtil<String> tcp = new TcpUtil<>("TrainingStatic", MfogManager.MODEL_STORE_PORT, model::iterator, null, null);
        tcp.client();
    }
}
