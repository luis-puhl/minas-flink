package br.ufscar.dc.ppgcc.gsdr.minas.kmeans;

import org.apache.samoa.instances.DenseInstance;
import org.apache.samoa.instances.Instance;
import org.apache.samoa.moa.cluster.Cluster;
import org.apache.samoa.moa.cluster.Clustering;
import org.apache.samoa.moa.cluster.SphereCluster;
import org.apache.samoa.moa.clusterers.KMeans;

import java.util.Arrays;
import java.util.LinkedList;
import java.util.List;
import java.util.Random;

public class MoaKmeans {
    public static double[][] POINTS = {
        {-14.22, -48.01}, {-22.78, 37.10}, {56.18, -42.99}, {35.04, 50.29},
        {-9.53, -46.26}, {-34.35, 48.25}, {55.82, -57.49}, {21.03, 54.64},
        {-13.63, -42.26}, {-36.57, 32.63}, {50.65, -52.40}, {24.48, 34.04},
        {-2.69, -36.02}, {-38.80, 36.58}, {24.00, -53.74}, {32.41, 24.96},
        {-4.32, -56.92}, {-22.68, 29.42}, {59.02, -39.56}, {24.47, 45.07},
        {5.23, -41.20}, {-23.00, 38.15}, {44.55, -51.50}, {14.62, 59.06},
        {7.41, -56.05}, {-26.63, 28.97}, {47.37, -44.72}, {29.07, 51.06},
        {0.59, -31.89}, {-39.09, 20.78}, {42.97, -48.98}, {34.36, 49.08},
        {-21.91, -49.01}, {-46.68, 46.04}, {48.52, -43.67}, {30.05, 49.25},
        {4.03, -43.56}, {-37.85, 41.72}, {38.24, -48.32}, {20.83, 57.85}
    };
    private static class Tuple2<A, B>{
        public A a;
        public B b;
        public Tuple2(A a, B b) {
            this.a = a;
            this.b = b;
        }
    }
//    public static final double MIN_VARIANCE = 1e-50;
//    private final static double EPSILON = 0.00005;
//
//    /**
//        micro cluster, as defined by Aggarwal et al, On Clustering Massive Data
//        Streams: A Summarization Praradigm in the book "Data streams: models and
//        algorithms", by Charu C Aggarwal
//
//        @article{
//            title = {Data Streams: Models and Algorithms},
//            author = {Aggarwal, Charu C.},
//            year = {2007},
//            publisher = {Springer Science+Business Media, LLC},
//            url = {http://ebooks.ulb.tu-darmstadt.de/11157/},
//            institution = {eBooks [http://ebooks.ulb.tu-darmstadt.de/perl/oai2] (Germany)},
//        }
//
//        DEFINITION A micro-cluster for a set of d-dimensional points Xi,. .Xi, with
//        timestamps ~. . .T,, is the (2-d+3)tuple (CF2", CFlX CF2t, CFlt, n), wherein
//        CF2" and CFlX each correspond to a vector of d entries. The definition of
//        each of these entries is as follows:
//            - For each dimension, the sum of the squares of the data values is
//              maintained in CF2". Thus, CF2" contains d values. The p-th entry of
//              CF2" is equal to \sum_j=1^n(x_i_j)^2
//            - For each dimension, the sum of the data values is maintained in C F l
//              X . Thus, CFIX contains d values. The p-th entry of CFIX is equal to
//              \sum_j=1^n x_i_j
//            - The sum of the squares of the time stamps Ti,. .Tin maintained in CF2t
//            - The sum of the time stamps Ti, . . .Tin maintained in CFlt.
//            - The number of data points is maintained in n.
//     */
//    static class CFClusterImp extends CFCluster {
//        // protected double N; // Number of points in the cluster.
//        // public double[] LS; // Linear sum of all the points added to the cluster.
//        // public double[] SS; // Squared sum of all the points added to the cluster.
//        public CFClusterImp(com.yahoo.labs.samoa.instances.Instance inst, int dimensions) {
//            super(inst, dimensions);
//        }
//
//        @Override
//        public CFCluster getCF() {
//            return null;
//        }
//
//        @Override
//        public double getInclusionProbability(com.yahoo.labs.samoa.instances.Instance instance) {
//            return 0;
//        }
//
//        @Override
//        public double getRadius() {
//            return 0;
//        }
//    }
//
//    static class MinasCluster{
//        protected double LST;
//        protected double SST;
//        protected String classe;
//        protected CFClusterImp cfClusterImp;
//        public MinasCluster(long timestamp, String classe, Instance instance) {
//            this.classe = classe;
//            this.LST = timestamp;
//            this.SST = timestamp * timestamp;
//            MinasInstance inst = new MinasInstance();
//            this.cfClusterImp = new CFClusterImp(inst);
//        }
//    }
    public static void main(String[] args) {
        Cluster[] clusters = new Cluster[Math.min(POINTS.length / 10, 100)];
        List<Cluster> points = new LinkedList<>();
        int i = 0;
        for (; i < clusters.length; i++) {
            double[] point = POINTS[i];
            points.add(new SphereCluster(point, 1.0));
            //
            clusters[i] = new SphereCluster(point, Double.MAX_VALUE);
        }
        for (; i < POINTS.length; i++) {
            double[] point = POINTS[i];
            points.add(new SphereCluster(point, 1.0));
        }
        Clustering clustering = KMeans.kMeans(clusters, points);
        StringBuilder sb = new StringBuilder();
        clustering.getDescription(sb, 0);
        System.out.println(sb.toString());
        for (int j = 0; j < clustering.size(); j++) {
            Cluster cluster = clustering.get(j);
            System.out.println(cluster.getInfo() + " " + Arrays.toString(cluster.getCenter()));
        }
    }
}
