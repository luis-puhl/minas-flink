package br.ufscar.dc.gsdr.mfog.util;

import java.util.Random;

public class Double64VsFloat32 {

    public static final int N_CLUSTERS = 100;
    public static final int DIMENSIONS = 22;
    public static final int N_EXAMPLES = 653457;
    static double[][] clusters;
    static double[][] examples;
    static float[][] clustersF;
    static float[][] examplesF;

    public static void main(String[] args) {
        long seed = DIMENSIONS * N_CLUSTERS * N_EXAMPLES;
        System.out.println("Test with DIMENSIONS="+DIMENSIONS+"; N_CLUSTERS="+N_CLUSTERS+" N_EXAMPLES="+N_EXAMPLES+"; seed="+seed+";");
        Random random = new Random(seed);
        long start = System.currentTimeMillis();
        clusters = new double[N_CLUSTERS][];
        clustersF = new float[N_CLUSTERS][];
        for (int i = 0; i < clusters.length; i++) {
            clusters[i] = new double[DIMENSIONS];
            clustersF[i] = new float[DIMENSIONS];
            for (int j = 0; j < clusters[i].length; j++) {
                clusters[i][j] = random.nextDouble();
                clustersF[i][j] = (float) clusters[i][j];
            }
        }
        examples = new double[N_EXAMPLES][];
        examplesF = new float[N_EXAMPLES][];
        for (int i = 0; i < examples.length; i++) {
            examples[i] = new double[DIMENSIONS];
            examplesF[i] = new float[DIMENSIONS];
            for (int j = 0; j < examples[i].length; j++) {
                examples[i][j] = random.nextDouble();
                examplesF[i][j] = (float) examples[i][j];
            }
        }
        long allocate = System.currentTimeMillis();
        double allocateTime = (allocate - start);
        System.out.println("double64 allocateTime=" + allocateTime + "ms\n");
        double d = double64();
        System.out.println();
        double f = float32();
        System.out.println("d-f="+(d-f)+" f-d="+(f-d)+" d/f="+(d/f)+" f/d="+(f/d));
    }
    static double double64(){
        long start = System.currentTimeMillis();
        double sumDist = 0;
        for (double[] example : examples) {
            double minDistance = Double.MAX_VALUE;
            for (double[] cluster : clusters) {
                double dist = 0;
                for (int k = 0; k < example.length && k < cluster.length; k++) {
                    double diff = cluster[k] - example[k];
                    dist += diff * diff;
                }
                dist = Math.sqrt(dist);
                if (dist < minDistance) {
                    minDistance = dist;
                }
            }
            sumDist += minDistance;
        }
        double avgDist = sumDist / examples.length;
        long run = System.currentTimeMillis();
        System.out.println("double64 sumDist=" + sumDist + " avgDist=" + avgDist);
        double runTime = (run - start);
        System.out.println("double64 runTime=" + runTime + "ms");
        return runTime;
    }
    static double float32(){
        long start = System.currentTimeMillis();
        float sumDist = 0;
        for (float[] example : examplesF) {
            float minDistance = Float.MAX_VALUE;
            for (float[] cluster : clustersF) {
                float dist = 0;
                for (int k = 0; k < example.length && k < cluster.length; k++) {
                    float diff = cluster[k] - example[k];
                    dist += diff * diff;
                }
                dist = (float) Math.sqrt(dist);
                if (dist < minDistance) {
                    minDistance = dist;
                }
            }
            sumDist += minDistance;
        }
        float avgDist = sumDist / examples.length;
        long run = System.currentTimeMillis();
        System.out.println("float32 sumDist=" + sumDist + " avgDist=" + avgDist);
        double runTime = (run - start);
        System.out.println("float32 runTime=" + runTime + "ms");
        return runTime;
    }
}
