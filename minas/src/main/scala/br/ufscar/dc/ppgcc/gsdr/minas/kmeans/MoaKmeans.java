package br.ufscar.dc.ppgcc.gsdr.minas.kmeans;

import org.apache.samoa.instances.DenseInstance;
import org.apache.samoa.moa.cluster.Cluster;
import org.apache.samoa.moa.cluster.Clustering;
import org.apache.samoa.moa.cluster.SphereCluster;
import org.apache.samoa.moa.clusterers.KMeans;
import org.apache.samoa.moa.clusterers.clustream.Clustream;

import java.util.Arrays;
import java.util.LinkedList;
import java.util.List;

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
        {4.03, -43.56}, {-37.85, 41.72}, {38.24, -48.32}, {20.83, 57.85},
        {-14.22, -48.01}, {-22.78, 37.10}, {56.18, -42.99}, {35.04, 50.29},
        {-9.53, -46.26}, {-34.35, 48.25}, {55.82, -57.49}, {21.03, 54.64},
        {-13.63, -42.26}, {-36.57, 32.63}, {50.65, -52.40}, {24.48, 34.04},
        {-2.69, -36.02}, {-38.80, 36.58}, {24.00, -53.74}, {32.41, 24.96},
        {-4.32, -56.92}, {-22.68, 29.42}, {59.02, -39.56}, {24.47, 45.07},
        {5.23, -41.20}, {-23.00, 38.15}, {44.55, -51.50}, {14.62, 59.06},
        {7.41, -56.05}, {-26.63, 28.97}, {47.37, -44.72}, {29.07, 51.06},
        {0.59, -31.89}, {-39.09, 20.78}, {42.97, -48.98}, {34.36, 49.08},
        {-21.91, -49.01}, {-46.68, 46.04}, {48.52, -43.67}, {30.05, 49.25},
        {4.03, -43.56}, {-37.85, 41.72}, {38.24, -48.32}, {20.83, 57.85},
    };
    private static class Tuple2<A, B>{
        public A a;
        public B b;
        public Tuple2(A a, B b) {
            this.a = a;
            this.b = b;
        }
    }
    public static String sphereClusterToString(SphereCluster cluster) {
        return (
            "SphereCluster(" +
            "id=" + (int) cluster.getId() + ", " +
            "classSimpleName=" + cluster.getClass().getSimpleName() + ", " +
            "weight=" + cluster.getWeight() + ", " +
            "radius=" + cluster.getRadius() + ", " +
            "center=" + Arrays.toString(cluster.getCenter()) +
            ")"
        );
    }

    public static void main(String[] args) {
        kmeans(POINTS);
        clustream(POINTS);
    }

    public static Clustering clustream(double[][] points) {
        Clustream clustream = new Clustream();
        clustream.prepareForUse();
        for (double[] point: points) {
            DenseInstance instance = new DenseInstance(1, point);
            clustream.trainOnInstanceImpl(instance);
        }
        Clustering clustering = clustream.getMicroClusteringResult();
        System.out.println("Clustream k=" + clustering.size());
        for (int j = 0; j < clustering.size(); j++) {
            SphereCluster cluster = (SphereCluster) clustering.get(j);
            System.out.println(sphereClusterToString(cluster));
        }
        // additionally, filters clusters with less than 3 points
        return clustering;
    }

    public static double[][] kmeans(double[][] points) {
        int k = Math.min(points.length / 10, 100);
        System.out.println("k-means k=" + k);
        Cluster[] clusters = new Cluster[k];
        List<Cluster> dataPoints = new LinkedList<>();
        int i = 0;
        for (; i < k; i++) {
            double[] point = points[i];
            dataPoints.add(new SphereCluster(point, 1.0));
            //
            clusters[i] = new SphereCluster(point, Double.MAX_VALUE);
        }
        for (; i < points.length; i++) {
            double[] point = points[i];
            dataPoints.add(new SphereCluster(point, 1.0));
        }
        Clustering clustering = KMeans.kMeans(clusters, dataPoints);
        double[][] clustersCenters = new double[clustering.size()][points[0].length];
        for (int j = 0; j < clustering.size(); j++) {
            SphereCluster cluster = (SphereCluster) clustering.get(j);
            System.out.println(sphereClusterToString(cluster));
            clustersCenters[j] = cluster.getCenter();
        }
        return clustersCenters;
    }
}
