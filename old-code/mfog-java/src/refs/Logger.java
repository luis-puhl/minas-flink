package br.ufscar.dc.gsdr.mfog.util;

import java.io.PrintWriter;
import java.io.StringWriter;
import java.util.HashSet;
import java.util.Set;

public class Logger extends Log.Logger {
    static Set<String> filterServices = new HashSet<>();
    String serviceName;

    Logger(String serviceName) {
        this.serviceName = serviceName;
    }

    public String getServiceName() {
        return serviceName;
    }

    public static String getLoggerName(Class<?> kls, Class<?>... parameterTypes) {
        String simpleName = kls.getSimpleName();
        StringBuilder sb = new StringBuilder(simpleName);
        if (parameterTypes != null && parameterTypes.length > 0) {
            sb.append("<");
            for (int i = 0; i < parameterTypes.length; i++) {
                if (i != 0) {
                    sb.append(", ");
                }
                if (parameterTypes[i] != null) {
                    sb.append(parameterTypes[i].getSimpleName());
                } else {
                    sb.append("null");
                }
            }
            sb.append(">");
        }
        return sb.toString();
    }

    @Deprecated
    public static Logger getLogger(Class<?> kls, Class<?>... parameterTypes) {
        String simpleName = kls.getSimpleName();
        if (filterServices.contains(simpleName)) {
            return new NoLogger(simpleName);
        }
        return new Logger(getLoggerName(kls, parameterTypes));
    }

    public void error(Throwable msg) {
        this.log(Log.LEVEL_ERROR, serviceName, "", msg);
    }
    public void error(String msg, Throwable exp) {
        this.log(Log.LEVEL_ERROR, serviceName, msg, exp);
    }
    public void warn(Object msg) {
        this.log(Log.LEVEL_WARN, serviceName, msg.toString(), null);
    }
    public void info(Object msg) {
        this.log(Log.LEVEL_INFO, serviceName, msg.toString(), null);
    }
    public void debug(Object msg) {
        this.log(Log.LEVEL_DEBUG, serviceName, msg.toString(), null);
    }
    public void trace(Object msg) {
        this.log(Log.LEVEL_TRACE, serviceName, msg.toString(), null);
    }

    public void log(int level, String category, String message, Throwable exp) {
        if (exp != null) {
            StackTraceElement[] stackTrace = exp.getStackTrace();
            StackTraceElement trace = stackTrace[0];
            StringBuilder sb = new StringBuilder(message)
                .append('\n')
                .append(exp.getMessage())
                .append("\n        at ")
                .append(trace.getClassName())
                .append(".").append(trace.getMethodName())
                .append("(").append(trace.getFileName())
                .append(":").append(trace.getLineNumber()).append(")\n");
            StringWriter writer = new StringWriter(256);
            exp.printStackTrace(new PrintWriter(writer));
            sb.append('\n');
            sb.append(writer.toString().trim());
//            for (StackTraceElement traceI : stackTrace) {
//                if (traceI.getClassName().startsWith("br.ufscar.dc.gsdr.mfog")) {
//                    sb.append("        at ")
//                        .append(traceI.getClassName())
//                        .append(".").append(traceI.getMethodName())
//                        .append("(").append(traceI.getFileName())
//                        .append(":").append(traceI.getLineNumber()).append(")\n");
//                }
//            }
            message = sb.toString();
        }
        String levelString = "LVL";
        switch (level) {
            case Log.LEVEL_ERROR:
                levelString = " ERROR: ";
                break;
            case Log.LEVEL_WARN:
                levelString = "  WARN: ";
                break;
            case Log.LEVEL_INFO:
                levelString = "  INFO: ";
                break;
            case Log.LEVEL_DEBUG:
                levelString = " DEBUG: ";
                break;
            case Log.LEVEL_TRACE:
                levelString = " TRACE: ";
                break;
        }
        if (category == null) {
            category = serviceName;
        }
        long now = System.currentTimeMillis();
        String formatted = String.format("%1$tFT%1$tT.%1$tL %2$-5s %3$s %4$s", now, levelString, category, message);
        System.out.println(formatted);
        System.out.flush();
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

    public static void setDefaultLogger() {
        Log.setLogger(new Log.Logger());
    }
}
