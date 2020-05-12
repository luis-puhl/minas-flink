package br.ufscar.dc.gsdr.mfog;

import br.ufscar.dc.gsdr.mfog.structs.Cluster;
import br.ufscar.dc.gsdr.mfog.structs.Message;
import br.ufscar.dc.gsdr.mfog.structs.Serializers;
import br.ufscar.dc.gsdr.mfog.util.MfogManager;
import br.ufscar.dc.gsdr.mfog.util.TimeIt;
import com.esotericsoftware.kryonet.Client;
import org.slf4j.LoggerFactory;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.util.List;
import java.util.stream.Collectors;

public class TrainingStatic {
    static final org.slf4j.Logger log = LoggerFactory.getLogger(TrainingStatic.class);

    public static void main(String[] args) throws Exception {

        String path = "datasets" + File.separator + "models" + File.separator + "offline.csv";
        BufferedReader in = new BufferedReader(new FileReader(path));
        List<Cluster> model = in.lines().skip(1).limit(100).map(Cluster::fromMinasCsv).collect(Collectors.toList());
        //
        Client client = new Client();
        Serializers.registerMfogStructs(client.getKryo());
        client.start();
        log.info("connecting to " + MfogManager.SERVICES_HOSTNAME + ":" + MfogManager.MODEL_STORE_PORT);
        client.connect(5000, MfogManager.SERVICES_HOSTNAME, MfogManager.MODEL_STORE_PORT);
        int i = 0;
        TimeIt timeIt = new TimeIt().start();
        client.sendTCP(new Message(Message.Intentions.SEND_ONLY));
        log.info("Sending");
        for (Cluster cluster : model) {
            client.sendTCP(cluster);
            i++;
        }
        client.sendTCP(new Message(Message.Intentions.DONE));
        client.close();
        //
        log.info(timeIt.finish(i));
    }
}
