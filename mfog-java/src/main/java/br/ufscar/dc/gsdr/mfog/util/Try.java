package br.ufscar.dc.gsdr.mfog.util;

public class Try<T> {
    public interface Tryable<T> {
        T apply() throws Exception;
    }
    public interface TryableVoid {
        void apply() throws Exception;
    }
    static Logger LOG = Logger.getLogger("Try");

    public static <U> Try<U> apply(Tryable<U> tryable, Logger LOG) {
        return new Try<>(tryable, LOG);
    }
    public static <U> Try<U> apply(Tryable<U> tryable) {
        return apply(tryable, LOG);
    }

    public static Try<Integer> apply(TryableVoid tryable) {
        return apply(tryable, LOG);
    }
    public static Try<Integer> apply(TryableVoid tryable, Logger LOG) {
        return new Try<>(() -> {
            tryable.apply();
            return 1;
        }, LOG);
    }

    public Tryable<T> tryable;
    public Exception error;
    public T get;
    public boolean failed;
    private Try(Tryable<T> tryable, Logger LOG) {
        this.tryable = tryable;
        try {
            get = tryable.apply();
            failed = false;
        } catch (Exception e) {
            LOG.error(e);
            error = e;
            failed = true;
        }
    }
}