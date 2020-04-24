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
        TcpUtil<String> server = new TcpUtil<>("ModelStore", MfogManager.MODEL_STORE_PORT, () -> {
            LOG.info("current model size =" + model.size());
            return model.stream();
        }, (s) -> {
            model.stream().limit(1).forEach((i) -> {
                if (i.equals(s)) LOG.info("current model size =" + model.size());
            });
            return s;
        }, model);
        server.server();
    }

    /*
    static void sender() {
        ServerSocket senderServer;
        if ((senderServer = Try.apply(() -> new ServerSocket(MfogManager.MODEL_STORE_PORT)).get) == null) return;
        //
        final Logger LOG = LoggerFactory.getLogger(className);
        LOG.info("Sender ready");
        for (int i = 0; i < 3; i++) {
            Socket socket;
            if ((socket = Try.apply(senderServer::accept).get) == null) break;
            long start = System.currentTimeMillis();
            LOG.info("sender connected");
            OutputStream outputStream;
            if ((outputStream = Try.apply(socket::getOutputStream).get) == null) {
                Try.apply(socket::close);
                break;
            }
            PrintStream out = new PrintStream(outputStream);
            LOG.info("sending " + model.get(1));
            model.forEach(out::println);
            out.flush();
            Try.apply(socket::close);
            LOG.info("sent " + model.size() + " items in " + (System.currentTimeMillis() - start) * 10e-4 + "s");
        }
        Try.apply(senderServer::close);
  }

    static void receiver() {
        ServerSocket receiverServer;
        if ((receiverServer = Try.apply(() -> new ServerSocket(MfogManager.MODEL_STORE_INTAKE_PORT)).get) == null) return;
        LOG.info("Receiver ready");
        for (int i = 0; i < 3; i++) {
            Socket socket;
            if ((socket = Try.apply(receiverServer::accept).get) == null) break;
            LOG.info("Receiver connected\nappending to " + model.size());
            InputStream inputStream;
            if ((inputStream = Try.apply(socket::getInputStream).get) == null) {
                Try.apply(socket::close);
                break;
            }
            BufferedReader read = new BufferedReader(new InputStreamReader(inputStream));
            read.lines().forEach(model::add);
            Try.apply(socket::close);
            LOG.info("total " + model.size());
        }
        Try.apply(receiverServer::close);
  }
     */

}
