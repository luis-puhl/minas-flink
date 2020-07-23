#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <err.h>

#include "../minas/minas.h"
#include "../util/loadenv.h"

Cluster *kMeansInit(int k, Cluster clusters[], int dimension, Point *examples[], int initialClusterId, char label, char category, FILE *timing, char *executable) {
    clock_t start = clock();
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
            modelCl->center[d] = examples[i]->value[d];
            modelCl->pointSum[d] = 0.0;
            modelCl->pointSqrSum[d] = 0.0;
        }
    }
    if (timing) {
        PRINT_TIMING(timing, executable, 1, start, k);
    }
    return clusters;
}

Cluster *kMeans(int k, Cluster clusters[], int dimension, Point *examples[], int nExamples, FILE *timing, char *executable) {
    clock_t start = clock();
    //
    Match *groupMatches = malloc(nExamples * sizeof(Match));
    double globalInnerDistance = dimension;
    double prevGlobalInnerDistance = dimension + 1;
    double improvement = 1;
    // recommended improvement threshold is 1.0E-05
    // kyoto goes to zero after *improvement > (1.0E-08)*
    for (int iter = 0; iter < 100 && improvement > 0; iter++) {
        prevGlobalInnerDistance = globalInnerDistance;
        globalInnerDistance = 0;
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
        // distances
        for (int exIndx = 0; exIndx < nExamples; exIndx++) {
            // classify(dimension, model, &(group[i]), &m);
            Point *ex = examples[exIndx];
            Match *match = &(groupMatches[exIndx]);
            // printf("classify %d %d\n", exIndx, group[exIndx].id);
            match->distance = (double) dimension;
            match->pointId = ex->id;
            for (int clIndx = 0; clIndx < k; clIndx++) {
                Cluster *cl = &(clusters[clIndx]);
                double distance = MNS_distance(ex->value, cl->center, dimension);
                if (match->distance > distance) {
                    match->cluster = cl;
                    // match->clusterId = cl->id;
                    // match->clusterLabel = cl->label;
                    // match->clusterRadius = cl->radius;
                    // match->secondDistance = match->distance;
                    match->distance = distance;
                }// else if (distance <= match->secondDistance) {
                //    match->secondDistance = distance;
                // }
            }
            // match->label = match->distance <= match->clusterRadius ? match->clusterLabel : '-';
            // update cluster
            // printf("update cluster %d -> %p\n", match->clusterId, match->cluster);
            match->cluster->matches++;
            // if (match->distance > match->cluster->distancesMax) {
            //     match->cluster->distancesMax = match->distance;
            // }
            match->cluster->distancesSum += match->distance;
            match->cluster->distancesSqrSum += match->distance * match->distance;
            for (int d = 0; d < dimension; d++) {
                match->cluster->pointSum[d] += ex->value[d];
                match->cluster->pointSqrSum[d] += ex->value[d] * ex->value[d];
            }
            globalInnerDistance += match->distance;
        }
        // assing new center
        for (int clIdx = 0; clIdx < k; clIdx++) {
            Cluster *cl = &clusters[clIdx];
            if (cl->matches == 0) continue;
            // if (cl->matches == 0) {
            //     for (int d = 0; d < dimension; d++) {
            //         // cl->center[d] = 0;
            //         cl->pointSum[d] = 0;
            //         cl->pointSqrSum[d] = 0;
            //     }
            // }
            // printf("assing new center to %d (%le avg dist)\n", cl->id, cl->distancesSum / cl->matches);
            for (int d = 0; d < dimension; d++) {
                cl->center[d] = cl->pointSum[d] / cl->matches;
                cl->pointSum[d] = 0;
                cl->pointSqrSum[d] = 0;
            }
            cl->distancesSum = 0;
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
        improvement = (globalInnerDistance / prevGlobalInnerDistance) - 1;
        improvement = improvement >= 0 ? improvement : - improvement;
        printf(
            "[%3d] Global dist of %le (%le avg) (%le better)\n",
            iter, globalInnerDistance, globalInnerDistance / nExamples, improvement
        );
    }
    // update distances
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
    for (int exIndx = 0; exIndx < nExamples; exIndx++) {
        // classify(dimension, model, &(group[i]), &m);
        Point *ex = examples[exIndx];
        Match *match = &(groupMatches[exIndx]);
        // printf("classify %d %d\n", exIndx, group[exIndx].id);
        match->distance = (double) dimension;
        match->pointId = ex->id;
        for (int clIndx = 0; clIndx < k; clIndx++) {
            Cluster *cl = &(clusters[clIndx]);
            double distance = MNS_distance(ex->value, cl->center, dimension);
            if (match->distance > distance) {
                match->cluster = cl;
                // match->clusterId = cl->id;
                // match->clusterLabel = cl->label;
                // match->clusterRadius = cl->radius;
                // match->secondDistance = match->distance;
                match->distance = distance;
            }// else if (distance <= match->secondDistance) {
            //    match->secondDistance = distance;
            // }
        }
        // match->label = match->distance <= match->clusterRadius ? match->clusterLabel : '-';
        // update cluster
        // printf("update cluster %d -> %p\n", match->clusterId, match->cluster);
        match->cluster->matches++;
        if (match->distance > match->cluster->distancesMax) {
            match->cluster->distancesMax = match->distance;
        }
        match->cluster->distancesSum += match->distance;
        match->cluster->distancesSqrSum += match->distance * match->distance;
        for (int d = 0; d < dimension; d++) {
            match->cluster->pointSum[d] += ex->value[d];
            match->cluster->pointSqrSum[d] += ex->value[d] * ex->value[d];
        }
    }
    for (int clIdx = 0; clIdx < k; clIdx++) {
        Cluster *cl = &clusters[clIdx];
        if (cl->matches != 0) {
            cl->meanDistance = cl->distancesSum / cl->matches;
        }
        // cl->radius = 0;
        cl->radius = cl->distancesMax;
    }
    //
    free(groupMatches);
    if (timing) {
        PRINT_TIMING(timing, executable, 1, start, nExamples);
    }
    return clusters;
}
