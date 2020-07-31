#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <err.h>

#include "../minas/minas.h"
#include "../util/loadenv.h"

Cluster *kMeansInit(int dimension, int k, Cluster clusters[], int nExamples, Point examples[], int initialClusterId, char label, char category, FILE *timing, char *executable) {
    clock_t start = clock();
    if (nExamples < k) {
        errx(EXIT_FAILURE, "Not enough examples for clustering. Needed %d and got %d\n", k, nExamples);
    }
    for (int i = 0; i < k; i++) {
        Cluster *modelCl = &(clusters[i]);
        modelCl->id = initialClusterId + i;
        modelCl->label = label;
        modelCl->category = category;
        modelCl->time = 0;
        modelCl->matches = 0;
        modelCl->meanDistance = 0.0;
        modelCl->distancesMax = 0.0;
        modelCl->radius = 0.0;
        modelCl->center = malloc(dimension * sizeof(double));
        modelCl->pointSum = malloc(dimension * sizeof(double));
        modelCl->pointSqrSum = malloc(dimension * sizeof(double));
        for (int d = 0; d < dimension; d++) {
            modelCl->center[d] = examples[i].value[d];
            modelCl->pointSum[d] = 0.0;
            modelCl->pointSqrSum[d] = 0.0;
        }
    }
    if (timing) {
        PRINT_TIMING(timing, executable, 1, start, k);
    }
    return clusters;
}

Cluster *kMeans(int dimension, int k, Cluster clusters[], int nExamples, Point examples[], FILE *timing, char *executable) {
    clock_t start = clock();
    //
    // init clusters
    for (int i = 0; i < k; i++) {
        clusters[i].matches = 0;
        clusters[i].distancesMax = 0.0;
        clusters[i].meanDistance = 0.0;
        clusters[i].radius = 0.0;
        for (int d = 0; d < dimension; d++) {
            clusters[i].pointSum[d] = 0.0;
            clusters[i].pointSqrSum[d] = 0.0;
        }
    }
    //
    Match *groupMatches = malloc(nExamples * sizeof(Match));
    double globalInnerDistance = dimension;
    double prevGlobalInnerDistance = dimension + 1;
    double improvement = 1;
    // recommended improvement threshold is 1.0E-05
    // kyoto goes to zero after *improvement > (1.0E-08)*
    printf("kMeans with k = %3d and %5d examples\n", k, nExamples);
    for (int iter = 0; iter < 100 && improvement > 0; iter++) {
        prevGlobalInnerDistance = globalInnerDistance;
        globalInnerDistance = 0;
        // distances
        for (int exIndx = 0; exIndx < nExamples; exIndx++) {
            Point *ex = &(examples[exIndx]);
            Match *match = &(groupMatches[exIndx]);
            match->distance = (double) dimension;
            match->pointId = ex->id;
            for (int clIndx = 0; clIndx < k; clIndx++) {
                Cluster *cl = &(clusters[clIndx]);
                double distance = MNS_distance(ex->value, cl->center, dimension);
                if (match->distance > distance) {
                    match->cluster = cl;
                    match->distance = distance;
                }
            }
            // update cluster
            match->cluster->matches++;
            for (int d = 0; d < dimension; d++) {
                match->cluster->pointSum[d] += ex->value[d];
            }
            globalInnerDistance += match->distance;
        }
        // assing new center
        for (int i = 0; i < k; i++) {
            // if (k <= 10) printf("center %d: ", i);
            for (int d = 0; d < dimension; d++) {
                if (clusters[i].matches != 0) {
                    clusters[i].center[d] = clusters[i].pointSum[d] / clusters[i].matches;
                    // if (k <= 10) printf("%f, ", clusters[i].center[d]);
                }
                clusters[i].pointSum[d] = 0.0;
            }
            // if (k <= 10) printf("\n");
            clusters[i].matches = 0;
        }
        // if (k <= 10) printf("\n");
        improvement = (globalInnerDistance / prevGlobalInnerDistance) - 1;
        improvement = improvement >= 0 ? improvement : - improvement;
        printf(
            "\t[%3d] Global dist of %le (%le avg) (%le better)\n",
            iter, globalInnerDistance, globalInnerDistance / nExamples, improvement
        );
    }
    //
    free(groupMatches);
    if (timing) {
        PRINT_TIMING(timing, executable, 1, start, nExamples);
    }
    return clusters;
}

// // update distances
// for (int i = 0; i < nExamples; i++) {
//     double distance = MNS_distance(group[i].value, groupMatches[i].cluster->center, dimension);
//     groupMatches[i].distance = distance;
//     groupMatches[i].cluster->distancesSum += distance;
//     groupMatches[i].cluster->distancesSqrSum += distance * distance;
//     if (distance > groupMatches[i].cluster->distancesMax) {
//         groupMatches[i].cluster->distancesMax = distance;
//     }
//     globalInnerDistance += distance;
// }
// for (int clIdx = 0; clIdx < nClusters; clIdx++) {
//     Cluster *cl = &clusters[clIdx];
//     if (cl->matches != 0) {
//         cl->meanDistance = cl->distancesSum / cl->matches;
//     }
//     // cl->radius = 0;
//     cl->radius = cl->distancesMax;
// }
// update std-dev
// for (int i = 0; i < nExamples; i++) {
//     double diff = groupMatches[i].distance - groupMatches[i].cluster->meanDistance;
//     groupMatches[i].cluster->radius += diff * diff;
// }
// for (int clIdx = 0; clIdx < nClusters; clIdx++) {
//     clusters[clIdx].radius = sqrt(clusters[clIdx].radius / (clusters[clIdx].matches - 1));
//     // clusters[clIdx].matches = 0;
// }
// stop when iteration limit is reached or when improvement is less than 1%
