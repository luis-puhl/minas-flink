package br.ufscar.dc.gsdr.mfog.util;

public class Logger {
    String serviceName;

    static public Logger getLogger(String serviceName) {
        return new Logger(serviceName);
    }

    Logger(String serviceName) {
        this.serviceName = serviceName;
    }

    public void info(String msg) {
        this.log("INFO", msg);
    }
    public void error(String msg) {
        this.log("ERR", msg);
    }
    public void error(Exception exp) {
        StackTraceElement trace = exp.getStackTrace()[0];
        this.log("ERR", trace.getClassName() + ":" + trace.getLineNumber() + " " + exp.getMessage());
    }
    private void log(String level, String msg) {
        System.out.println(format(level, msg));
        System.out.flush();
    }
    public String format(String level, String msg) {
        return format(level, msg, System.currentTimeMillis());
    }
    public String format(String level, String msg, long now) {
        // 1969-12-31T21:00:00.000 LVL   service message
        return String.format("%1$tFT%1$tT.%1$tL %2$-5s %3$s %4$s", now, level, serviceName, msg);
    }
}
