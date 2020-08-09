#ifndef _KMEANS_C
#define _KMEANS_C

#include <stdio.h>
#include <stdlib.h>
// #include <string.h>
// #include <err.h>
#include <math.h>
#include <time.h>
#include <ctype.h>

#include "./base.h"

Cluster* kMeansInit(Params *params, Example trainingSet[], unsigned int trainingSetSize, unsigned int initalId) {
    Cluster *clusters = calloc(params->k, sizeof(Cluster));
    for (size_t i = 0; i < params->k; i++) {
        clusters[i].id = initalId + i;
        clusters[i].n_matches = 0;
        clusters[i].center = calloc(params->dim, sizeof(double));
        clusters[i].ls_valLinearSum = calloc(params->dim, sizeof(double));
        clusters[i].ss_valSquareSum = calloc(params->dim, sizeof(double));
        // clusters[i].valAverage = calloc(params->dim, sizeof(double));
        // clusters[i].valStdDev = calloc(params->dim, sizeof(double));
        for (size_t d = 0; d < params->dim; d++) {
            clusters[i].center[d] = trainingSet[i].val[d];
            clusters[i].ls_valLinearSum[d] = 0.0;
            clusters[i].ls_valLinearSum[d] = 0.0;
            // clusters[i].valAverage[d] = 0.0;
            // clusters[i].valStdDev[d] = 0.0;
        }
    }
    return clusters;
}

double kMeans(Params *params, Cluster* clusters, Example trainingSet[], unsigned int trainingSetSize) {
    clock_t start = clock();
    double improvement, prevGlobalDistance, globalDistance = params->dim * params->k * trainingSetSize * 2;
    unsigned int iteration = 0;
    do {
        prevGlobalDistance = globalDistance;
        globalDistance = 0.0;
        for (size_t i = 0; i < trainingSetSize; i++) {
            double minDist;
            Cluster *nearest = NULL;
            for (size_t k = 0; k < params->k; k++) {
                #ifdef SQR_DIST
                double dist = euclideanSqrDistance(params->dim, clusters[k].center, trainingSet[i].val);
                #else
                double dist = euclideanDistance(params->dim, clusters[k].center, trainingSet[i].val);
                #endif
                if (nearest == NULL || dist <= minDist) {
                    minDist = dist;
                    nearest = &clusters[k];
                }
            }
            #ifdef SQR_DIST
            minDist = sqrt(minDist);
            #endif
            globalDistance += minDist;
            nearest->n_matches++;
            for (size_t d = 0; d < params->dim; d++) {
                nearest->ls_valLinearSum[d] += trainingSet[i].val[d];
            }
        }
        for (size_t k = 0; k < params->k; k++) {
            for (size_t d = 0; d < params->dim; d++) {
                if (clusters[k].n_matches > 0)
                    clusters[k].center[d] = clusters[k].ls_valLinearSum[d] / clusters[k].n_matches;
                clusters[k].ls_valLinearSum[d] = 0.0;
                clusters[k].ss_valSquareSum[d] = 0.0;
            }
            clusters[k].n_matches = 0;
        }
        improvement = globalDistance - prevGlobalDistance;
        fprintf(stderr, "\t[%3u] k-Means %le -> %le (%+le)\n", iteration, prevGlobalDistance, globalDistance, improvement);
        if (improvement < 0)
            improvement = -improvement;
        iteration++;
    } while (improvement > params->precision && iteration < 100);
    printTiming(trainingSetSize);
    return globalDistance;
}

#endif // !_KMEANS_C
