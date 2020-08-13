#ifndef _BASE_C
#define _BASE_C

#include <stdio.h>
#include <stdlib.h>
// #include <string.h>
// #include <err.h>
#include <math.h>
// #include <time.h>
#include <ctype.h>

#include "./base.h"

char *printableLabel(char label) {
    if (isalpha(label) || label == '-') {
        char *ret = calloc(2, sizeof(char));
        ret[0] = label;
        return ret;
    }
    char *ret = calloc(20, sizeof(char));
    sprintf(ret, "%d", label);
    return ret;
}

double euclideanSqrDistance(unsigned int dim, double a[], double b[]) {
    double distance = 0;
    for (size_t d = 0; d < dim; d++) {
        distance += (a[d] - b[d]) * (a[d] - b[d]);
    }
    return distance;
}

double euclideanDistance(unsigned int dim, double a[], double b[]) {
    return sqrt(euclideanSqrDistance(dim, a, b));
}

double nearestClusterValStd(Params *params, Cluster clusters[], size_t nClusters, double val[], Cluster **nearest) {
    double minDist;
    *nearest = NULL;
    for (size_t k = 0; k < nClusters; k++) {
        // double dist = 0.0;
        // for (size_t d = 0; d < params->dim; d++) {
        //     double v = (clusters[k].center[d] - val[d]);
        //     dist += v * v;
        // }
        // dist = sqrt(dist);
        double dist = euclideanDistance(params->dim, val, clusters[k].center);
        if (*nearest == NULL || dist <= minDist) {
            minDist = dist;
            *nearest = &clusters[k];
        }
    }
    return minDist;
}

double nearestClusterVal(Params *params, Cluster clusters[], size_t nClusters, double val[], Cluster **nearest) {
    double minDist;
    *nearest = NULL;
    for (size_t k = 0; k < nClusters; k++) {
        double dist = 0.0;
        for (size_t d = 0; d < params->dim; d++) {
            double v = (clusters[k].center[d] - val[d]);
            // dist += fabs(v);
            dist += v * v;
            if (k > 0 && dist > minDist) break;
        }
        if (k == 0 || dist <= minDist) {
            minDist = dist;
            *nearest = &clusters[k];
        }
    }
    return sqrt(minDist);
    // // redo distance, because early skip
    // double dist = 0.0;
    // for (size_t d = 0; d < params->dim; d++) {
    //     double v = ((*nearest)->center[d] - val[d]);
    //     dist += v * v;
    // }
    // return sqrt(dist);
}

#endif // !_BASE_C
