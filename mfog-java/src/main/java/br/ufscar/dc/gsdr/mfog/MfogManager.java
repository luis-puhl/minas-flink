package br.ufscar.dc.gsdr.mfog;

import java.io.File;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;

public class MfogManager {
    static final String SERVICES_HOSTNAME = "192.168.15.11";
    static final int MODEL_STORE_PORT = 9997;
    public static final int SOURCE_MODULE_TEST_PORT = 9996;
    public static final int SINK_MODULE_TEST_PORT = 9995;
    public static final String dateString = LocalDateTime.now().format(DateTimeFormatter.ISO_DATE_TIME).replaceAll(":", "-");

    static void createDir(String jobName) {
        String outDir = "./out/" + jobName + "/" + dateString + "/";
        File dir = new File(outDir);
        if (!dir.exists()) {
            if (!dir.mkdirs()) throw new RuntimeException("Output directory '" + outDir +"'could not be created.");
        }
    }
}
