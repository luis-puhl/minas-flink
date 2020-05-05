package br.ufscar.dc.gsdr.mfog.util;

import java.io.File;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;

public class MfogManager {
    public static final String SERVICES_HOSTNAME = "192.168.15.13";
    public static final int SOURCE_TRAINING_DATA_PORT = 9999;
    public static final int MODEL_STORE_INTAKE_PORT = 9998;
    public static final int MODEL_STORE_PORT = 9997;
    public static final int SOURCE_TEST_DATA_PORT = 9996;
    public static final int SINK_MODULE_TEST_PORT = 9995;
    public static final int SOURCE_EVALUATE_DATA_PORT = 9994;
    public static final String dateString = LocalDateTime.now().format(DateTimeFormatter.ISO_DATE_TIME).replaceAll(":", "-");

    public static final boolean USE_GZIP = false;

    public static void createDir(String jobName) {
        String outDir = "./out/" + jobName + "/" + dateString + "/";
        File dir = new File(outDir);
        if (!dir.exists()) {
            if (!dir.mkdirs()) throw new RuntimeException("Output directory '" + outDir +"'could not be created.");
        }
    }
}
