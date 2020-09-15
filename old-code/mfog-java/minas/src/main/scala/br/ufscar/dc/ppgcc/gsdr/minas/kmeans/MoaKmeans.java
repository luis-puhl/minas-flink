package br.ufscar.dc.ppgcc.gsdr.minas.kmeans;

import org.apache.samoa.instances.DenseInstance;
import org.apache.samoa.moa.cluster.Cluster;
import org.apache.samoa.moa.cluster.Clustering;
import org.apache.samoa.moa.cluster.SphereCluster;
import org.apache.samoa.moa.clusterers.KMeans;
import org.apache.samoa.moa.clusterers.clustream.Clustream;

import java.util.*;
import java.util.stream.DoubleStream;

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
        int seed = 100;
                // (int) (Math.random() * 100);
        Random rnd = new Random(seed);

        int dimensions = Math.abs(rnd.nextInt(100));
        int length = Math.abs(rnd.nextInt((int) 10e4));
        System.out.println("seed=" + seed + " dimensions=" + dimensions + " length=" + length + " total=" + dimensions * length);
        DoubleStream doubleStream = rnd.doubles(dimensions * length, 0, 1);
        PrimitiveIterator.OfDouble doubles = doubleStream.iterator();

        double[][] POINTS = new double[length][dimensions];
        for (double[] point : POINTS) {
            for (int j = 0; j < point.length; j++) {
                point[j] = doubles.nextDouble();
            }
        }

        int k = Math.min(POINTS.length / 10, 100);

        Cluster[] kMeansInitByHead = kMeansInitByHead(POINTS, k);
        System.out.println("kMeansInitByHead k=" + k);
        System.out.println(Arrays.toString(summation(dimensions, kMeansInitByHead)));

        Clustering kMeans = kMeans(POINTS, kMeansInitByHead);
        System.out.println("kMeans k=" + kMeans.size());
        System.out.println(Arrays.toString(summation(dimensions, kMeans)));

        Clustering cluStream = cluStream(POINTS);
        System.out.println("cluStream k=" + cluStream.size());
        System.out.println(Arrays.toString(summation(dimensions, cluStream)));
    }

    public static double[] summation(int dimensions, Cluster[] clusters) {
        double[] sum = new double[dimensions];
        for (Cluster cluster : clusters) {
            double[] x = cluster.getCenter();
            for (int i = 0; i < x.length; i++) {
                sum[i] += x[i];
            }
        }
        return sum;
    }
    public static double[] summation(int dimensions, Clustering clustering) {
        double[] sum = new double[dimensions];
        for (Cluster cluster : clustering.getClustering()) {
            double[] x = cluster.getCenter();
            for (int i = 0; i < x.length; i++) {
                sum[i] += x[i];
            }
        }
        return sum;
    }

    public static Clustering cluStream(double[][] points) {
        Clustream clustream = new Clustream();
        clustream.prepareForUse();
        for (double[] point: points) {
            DenseInstance instance = new DenseInstance(1, point);
            clustream.trainOnInstanceImpl(instance);
        }
        // additionally, filters clusters with less than 3 points
        return clustream.getMicroClusteringResult();
    }

    public static Cluster[] kMeansInitByHead(double[][] points, int k) {
        Cluster[] clusters = new Cluster[k];
        for (int i = 0; i < k; i++) {
            clusters[i] = new SphereCluster(points[i], 0);
        }
        return clusters;
    }
    public static Clustering kMeans(double[][] points, Cluster[] seed) {
        List<Cluster> dataPoints = new LinkedList<>();
        for (double[] point : points) {
            dataPoints.add(new SphereCluster(point, 1.0));
        }
        return KMeans.kMeans(seed, dataPoints);
    }
}
