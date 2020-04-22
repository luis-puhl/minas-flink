package br.ufscar.dc.gsdr.mfog;

import br.ufscar.dc.gsdr.mfog.structs.Cluster;
import br.ufscar.dc.gsdr.mfog.util.ModelStoreAkka;

import java.io.*;
import java.net.InetAddress;
import java.net.Socket;
import java.util.Iterator;
import java.util.logging.Logger;
import java.util.stream.Stream;

public class TrainingStatic {
    static final Logger LOG = Logger.getLogger(ModelStoreAkka.class.getName());
    public static void main(String[] args) throws IOException {
        long start = System.currentTimeMillis();
        String path = "datasets" + File.separator + "models" + File.separator + "offline.csv";
        BufferedReader in = new BufferedReader(new FileReader(path));
        Stream<Cluster> model = in.lines().skip(1).map(Cluster::fromMinasCsv);
        Socket modelStoreSocket = new Socket(InetAddress.getByName("localhost"), 9998);
        LOG.info("connected");
        PrintStream outStream = new PrintStream(modelStoreSocket.getOutputStream());
        int i = 0;
        Iterator<Cluster> iterator = model.iterator();
        while (iterator.hasNext()) {
            outStream.println(iterator.next().json().toString());
            i++;
        }
        outStream.flush();
        modelStoreSocket.close();
        LOG.info("sent = " + i + " in " + (System.currentTimeMillis() - start) * 10e-4 + "s");
    }
}
