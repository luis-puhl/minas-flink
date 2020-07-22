#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <err.h>

#include "../minas/minas.h"
#include "../util/loadenv.h"

Cluster* kMeansInit(int nClusters, int dimension, Point examples[], int initialClusterId, char label, char category, FILE *timing, char *executable) {
    clock_t start = clock();
    Cluster *clusters = malloc(nClusters * sizeof(Cluster));
    for (int i = 0; i < nClusters; i++) {
        clusters[i].id = initialClusterId + i;
        clusters[i].center = malloc(dimension * sizeof(double));
        clusters[i].pointSum = malloc(dimension * sizeof(double));
        clusters[i].pointSqrSum = malloc(dimension * sizeof(double));
        for (int j = 0; j < dimension; j++) {
            clusters[i].center[j] = examples[i].value[j];
            clusters[i].pointSum[j] = 0.0;
            clusters[i].pointSqrSum[j] = 0.0;
        }
        clusters[i].label = label;
        clusters[i].category = category;
        clusters[i].time = 0;
        clusters[i].matches = 0;
        clusters[i].meanDistance = 0.0;
        clusters[i].radius = 0.0;
    }
    if (timing) {
        PRINT_TIMING(timing, executable, 1, start, nClusters);
    }
    return clusters;
}

Model *kMeans(Model *model, int nClusters, int dimension, Point examples[], int nExamples, FILE *timing, char *executable) {
    // Model *model = kMeansInit(nClusters, dimension, examples);
    Match match;
    // double *classifyDistances = malloc(model->size * sizeof(double));
    clock_t start = clock();
    //
    double globalDistance = dimension * 2.0, prevGlobalDistance, diffGD = 1.0;
    double newCenters[nClusters][dimension], distances[nClusters], sqrDistances[nClusters];
    int matches[nClusters];
    int maxIterations = 10;
    while (diffGD > 0.00001 && maxIterations-- > 0) {
        // setup
        prevGlobalDistance = globalDistance;
        globalDistance = 0.0;
        for (int i = 0; i < model->size; i++) {
            matches[model->vals[i].id] = 0;
            distances[model->vals[i].id] = 0.0;
            sqrDistances[model->vals[i].id] = 0.0;
            for (int d = 0; d < dimension; d++) {
                newCenters[model->vals[i].id][d] = 0.0;
            }
        }
        // distances
        for (int i = 0; i < nExamples; i++) {
            classify(dimension, model, &(examples[i]), &match);
            globalDistance += match.distance;
            distances[match.clusterId] += match.distance;
            sqrDistances[match.clusterId] += match.distance * match.distance;
            for (int d = 0; d < dimension; d++) {
                newCenters[match.clusterId][d] += examples[i].value[d];
            }
            matches[match.clusterId]++;
        }
        // new centers and radius
        for (int i = 0; i < model->size; i++) {
            Cluster *cl = &(model->vals[i]);
            cl->matches = matches[cl->id];
            // skip clusters that didn't move
            if (cl->matches == 0) continue;
            cl->time++;
            // avg of examples in the cluster
            double maxDistance = -1.0;
            for (int d = 0; d < dimension; d++) {
                cl->center[d] = newCenters[cl->id][d] / cl->matches;
                if (distances[cl->id] > maxDistance) {
                    maxDistance = distances[cl->id];
                }
            }
            cl->meanDistance = distances[cl->id] / cl->matches;
            /**
             * Radius is not clearly defined in the papers and original source code
             * So here is defined as max distance
             *  OR square distance sum divided by matches.
             **/
            // cl->radius = sqrDistances[cl->id] / cl->matches;
            cl->radius = maxDistance;
        }
        //
        diffGD = globalDistance / prevGlobalDistance;
        fprintf(stderr, "%s iter=%d, diff%%=%e (%e -> %e)\n", __FILE__, maxIterations, diffGD, prevGlobalDistance, globalDistance);
    }
    if (timing) {
        PRINT_TIMING(timing, executable, 1, start, nExamples);
    }
    return model;
}

/*
    public static Clustering kMeans(Cluster[] centers, List<? extends Cluster> data ) {
        int k = centers.length;

        int dimensions = centers[0].getCenter().length;

        ArrayList<ArrayList<Cluster>> clustering = new ArrayList<ArrayList<Cluster>>();
        for ( int i = 0; i < k; i++ ) {
            clustering.add( new ArrayList<Cluster>() );
        }

        int repetitions = 100;
        while ( repetitions-- >= 0 ) {
            // Assign points to clusters
            for ( Cluster point : data ) {
                double minDistance = distance( point.getCenter(), centers[0].getCenter() );
                int closestCluster = 0;
                for ( int i = 1; i < k; i++ ) {
                    double distance = distance( point.getCenter(), centers[i].getCenter() );
                    if ( distance < minDistance ) {
                        closestCluster = i;
                        minDistance = distance;
                    }
                }

                clustering.get( closestCluster ).add( point );
            }

            // Calculate new centers and clear clustering lists
            SphereCluster[] newCenters = new SphereCluster[centers.length];
            for ( int i = 0; i < k; i++ ) {
                newCenters[i] = calculateCenter( clustering.get( i ), dimensions );
                clustering.get( i ).clear();
            }
            centers = newCenters;
        }

        return new Clustering( centers );
    }
    private static SphereCluster calculateCenter( ArrayList<Cluster> cluster, int dimensions ) {
        double[] res = new double[dimensions];
        for ( int i = 0; i < res.length; i++ ) {
            res[i] = 0.0;
        }

        if ( cluster.size() == 0 ) {
            return new SphereCluster( res, 0.0 );
        }

        for ( Cluster point : cluster ) {
            double [] center = point.getCenter();
            for (int i = 0; i < res.length; i++) {
               res[i] += center[i];
            }
        }

        // Normalize
        for ( int i = 0; i < res.length; i++ ) {
            res[i] /= cluster.size();
        }

        // Calculate radius
        double radius = 0.0;
        for ( Cluster point : cluster ) {
            double dist = distance( res, point.getCenter() );
            if ( dist > radius ) {
                radius = dist;
            }
        }

        return new SphereCluster( res, radius );
    }
*/
