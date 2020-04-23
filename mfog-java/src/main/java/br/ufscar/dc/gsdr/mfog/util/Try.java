package br.ufscar.dc.gsdr.mfog.util;

public class Try<T> {
    public interface Tryable<T> {
        T apply() throws Exception;
    }
    public interface TryableVoid {
        void apply() throws Exception;
    }

    public static <U> Try<U> apply(Tryable<U> tryable) {
        return new Try<U>(tryable);
    }
    public static Try<Integer> apply(TryableVoid tryable) {
        return new Try<>(() -> {
            tryable.apply();
            return 1;
        });
    }

    public Tryable<T> tryable;
    public Exception error;
    public T get;
    public boolean failed;
    private Try(Tryable<T> tryable) {
        this.tryable = tryable;
        try {
            get = tryable.apply();
            failed = false;
        } catch (Exception e) {
            System.err.println(e);
            error = e;
            failed = true;
        }
    }
}