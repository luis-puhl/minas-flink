package br.ufscar.dc.gsdr.mfog;

import org.json.JSONArray;
import org.json.JSONObject;

import java.util.HashMap;
import java.util.Map;

public class Cluster {
    public static String CATEGORY_NORMAL = "normal";
    public static String CATEGORY_EXTENSION = "extension";
    public static String CATEGORY_NOVELTY = "novelty";
    public static String CATEGORY_NOVELTY_EXTENSION = "novelty extension";

    public static String CSV_HEADER = "id,label,category,matches,time,variance,center";
    // size,lblClasse,category,time,meanDistance,radius,center

    public static Cluster fromJson(String src) {
        return fromJson(new JSONObject(src));
    }
    public static Cluster fromJson(JSONObject json) {
        long id = json.getLong("id");
        double variance = json.getDouble("variance");
        String label = json.getString("label");
        String category = json.getString("category");
        long matches = json.getLong("matches");
        long time = json.getLong("time");
        JSONObject centerSrc = json.getJSONObject("center");
        Point center = Point.fromJson(centerSrc);
        return new Cluster(id, center, variance, label, category, matches, time);
    }

    public static Cluster fromMinasCsv(String line) {
        String[] metaAndCenter = line.split(",\\[");
        String meta = metaAndCenter[0];
        String centerString = metaAndCenter[1].replaceAll("[ \\]]", "");
        String[] split = meta.split(",");
        //
        long id                = Long.parseLong(split[0]);
        String label           = split[1];
        String category        = split[2];
        long matches           = Long.parseLong(split[3]);
        long time              = Long.parseLong(split[4]);
        double meanDistance    = Double.parseDouble(split[5]);
        double radius          = Double.parseDouble(split[6]);
        String[] centerStrings = centerString.split(",");
        //
        double[] center        = new double[centerStrings.length +1];
        for (int i = 0; i < centerStrings.length; i++) {
            center[i] = Double.parseDouble(centerStrings[i]);
        }
        return new Cluster(id, new Point(id, center, time), radius, label, category, matches, time);
    }

    public static Cluster apply(long id, Point center, double variance, String label){
        return apply(id, center, variance, label, Cluster.CATEGORY_NORMAL);
    }
    public static Cluster apply(long id, Point center, double variance, String label, String category){
        return apply(id, center, variance, label, category, 0);
        // matches: Long = 0
    }
    public static Cluster apply(long id, Point center, double variance, String label, String category, long matches){
        return apply(id, center, variance, label, category, matches, System.currentTimeMillis());
        // time: Long = System.currentTimeMillis()
    }
    public static Cluster apply(long id, Point center, double variance, String label, String category, long matches, long time){
        return new Cluster(id, center, variance, label, category, matches, time);
    }

    public long id;
    public Point center;
    public double variance;
    public String label;
    public String category;
    public long matches;
    public long time;
    private Cluster(long id, Point center, double variance, String label, String category, long matches, long time) {
        this.id = id;
        this.center = center;
        this.variance = variance;
        this.label = label;
        this.category = category;
        this.matches = matches;
        this.time = time;
    }

    public JSONObject json() {
        JSONObject obj = new JSONObject();
        obj.append("id", id);
        obj.append("center", center.json());
        obj.append("variance", variance);
        obj.append("label", label);
        obj.append("category", category);
        obj.append("matches", matches);
        obj.append("time", time);
        return obj;
    }
}
