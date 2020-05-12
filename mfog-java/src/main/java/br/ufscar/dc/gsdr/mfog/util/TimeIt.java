package br.ufscar.dc.gsdr.mfog.util;

public class TimeIt {
    long millis;
    long nano;

    public TimeIt start() {
        millis = System.currentTimeMillis();
        nano = System.nanoTime();
        return this;
    }

    public String finish(long items) {
        long millisDiff = System.currentTimeMillis() - millis;
        long nanoDiff = System.nanoTime() - nano;
        long speed = millisDiff == 0 || items == 0 ? 0 : items / millisDiff;
        return items + " items, " + millisDiff + " ms, " + nanoDiff + " ns, " + speed + " i/ms";
    }
}
