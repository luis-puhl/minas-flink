package br.ufscar.dc.gsdr.mfog.util;

import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;

public class Logger {
    String serviceName;

    static public Logger getLogger(String serviceName) {
        return new Logger(serviceName);
    }

    Logger(String serviceName) {
        this.serviceName = serviceName;
    }

    public void info(String msg) {
        String time = DateTimeFormatter.ISO_LOCAL_DATE_TIME.format(LocalDateTime.now());
        System.out.println(time + " " + serviceName + " " + msg);
    }
}
