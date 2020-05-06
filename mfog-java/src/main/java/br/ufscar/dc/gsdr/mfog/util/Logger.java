package br.ufscar.dc.gsdr.mfog.util;

import java.util.HashSet;
import java.util.Set;

public class Logger {
    static Set<String> filterServices = new HashSet<>();
    String serviceName;

    Logger(String serviceName) {
        this.serviceName = serviceName;
    }

    @Deprecated
    static public Logger getLogger(String serviceName) {
        if (filterServices.contains(serviceName)) {
            return new NoLogger(serviceName);
        }
        return new Logger(serviceName);
    }

    public static String getLoggerMame(Class<?> kls, Class<?>... parameterTypes) {
        String serviceName = kls.getSimpleName();
        StringBuilder sb = new StringBuilder(serviceName);
        sb.append("<");
        for (int i = 0; i < parameterTypes.length; i++) {
            if (i != 0) {
                sb.append(", ");
            }
            sb.append(parameterTypes[i].getSimpleName());
        }
        sb.append(">");
        return sb.toString();
    }

    public static Logger getLogger(Class<?> kls, Class<?>... parameterTypes) {
        String serviceName = kls.getSimpleName();
        if (filterServices.contains(serviceName)) {
            return new NoLogger(serviceName);
        }
        return getLogger(getLoggerMame(kls, parameterTypes));
    }

    static public Logger getLogger(Class<?> kls) {
        return getLogger(kls.getSimpleName());
    }

    public void debug(String s) {
    }

    public void info(Object msg) {
        this.log("INFO", msg.toString());
    }

    public void warn(Object msg) {
        this.log("WARN", msg.toString());
    }

    public void error(Object msg) {
        this.log("ERR", msg.toString());
    }

    public void error(Exception exp) {
        StackTraceElement[] stackTrace = exp.getStackTrace();
        StackTraceElement trace = stackTrace[0];
        StringBuilder sb = new StringBuilder(exp.getMessage());
        sb.append("\n        at ")
            .append(trace.getClassName())
            .append(".").append(trace.getMethodName())
            .append("(").append(trace.getFileName())
            .append(":").append(trace.getLineNumber()).append(")\n");
        for (StackTraceElement traceI : stackTrace) {
            if (traceI.getClassName().startsWith("br.ufscar.dc.gsdr.mfog")) {
                sb.append("        at ")
                    .append(traceI.getClassName())
                    .append(".").append(traceI.getMethodName())
                    .append("(").append(traceI.getFileName())
                    .append(":").append(traceI.getLineNumber()).append(")\n");
            }
        }
        this.log("ERR", sb.toString());
        // exp.printStackTrace();
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

    public static class NoLogger extends Logger {
        NoLogger(String serviceName) {
            super(serviceName);
        }

        public void info(Object msg) {
        }

        public void warn(Object msg) {
        }

        public void error(Object msg) {
        }

        public void error(Exception exp) {
        }

        public void debug(String s) {
        }
    }
}
