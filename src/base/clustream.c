#ifndef _CLUSTREAM_C
#define _CLUSTREAM_C

#include <stdio.h>
#include <stdlib.h>
// #include <string.h>
// #include <err.h>
#include <math.h>
#include <time.h>
// #include <ctype.h>

#include "./base.h"
#include "./minas.h"
#include "./kmeans.h"

Cluster* cluStream(Params *params, Example trainingSet[], unsigned int trainingSetSize, unsigned int initalId) {
    clock_t start = clock();
    /**
     * m        Defines the maximum number of micro-clusters used in CluStream
     * horizon  Defines the time window to be used in CluStream
     * t        Maximal boundary factor (=Kernel radius factor)
     *          When deciding to add a new data point to a micro-cluster,
     *          the maximum boundary is defined as a factor of t of the RMS
     *          deviation of the data points in the micro-cluster from the centroid.
     * k        Number of macro-clusters to produce using weighted k-means. NULL disables automatic reclustering.
    **/
    // int alphaStorageFator = 2;
    // double horizon = 3;
    // double t = params->radiusF;
    //
    double k = params->k;
    params->k = params->cluStream_q_maxMicroClusters;
    double initNumbers = params->cluStream_q_maxMicroClusters * params->minExamplesPerCluster;
    Cluster *microClusters = kMeansInit(params, trainingSet, initNumbers, initalId);
    kMeans(params, microClusters, trainingSet, params->cluStream_q_maxMicroClusters);
    // restore k
    params->k = k;
    // fill radius with nearest cl dist
    for (size_t clId = 0; clId < params->cluStream_q_maxMicroClusters; clId++) {
        microClusters[clId].n_matches = 1;
        microClusters[clId].timeLinearSum = 1;
        microClusters[clId].timeSquareSum = 1;
        //
        // initial radius is distance to nearest cluster
        Cluster *nearest = NULL;
        for (size_t nearId = 0; nearId < params->cluStream_q_maxMicroClusters; nearId++) {
            double dist = euclideanDistance(params->dim, microClusters[clId].center, microClusters[clId].center);
            if (nearest == NULL || microClusters[clId].radius > dist) {
                microClusters[clId].radius = dist;
                nearest = &microClusters[clId];
            }
        }
    }
    // consume stream
    size_t nextClusterId = params->cluStream_q_maxMicroClusters;
    for (size_t i = 0; i < trainingSetSize; i++) {
        Example *ex = &trainingSet[i];
        //
        Cluster *nearest = NULL;
        double minDist;
        for (size_t clId = 0; clId < params->cluStream_q_maxMicroClusters; clId++) {
            double dist = euclideanDistance(params->dim, microClusters[clId].center, ex->val);
            if (nearest == NULL || minDist > dist) {
                minDist = dist;
                nearest = &microClusters[clId];
            }
        }
        if (nearest->radius < minDist) {
            // add example to cluster
            nearest->n_matches++;
            nearest->timeLinearSum += i;
            nearest->timeSquareSum += i * i;
            nearest->radius = 0;
            nearest->distanceAvg = 0;
            for (size_t d = 0; d < params->dim; d++) {
                nearest->ls_valLinearSum[d] += ex->val[d];
                nearest->ss_valSquareSum[d] += ex->val[d] * ex->val[d];
                //
                nearest->center[d] = nearest->ls_valLinearSum[d] / nearest->n_matches;
                //
                nearest->distanceAvg += nearest->ls_valLinearSum[d] / nearest->n_matches;
                //
                double variance = nearest->ls_valLinearSum[d] / nearest->n_matches;
                variance *= variance;
                variance -= nearest->ss_valSquareSum[d] / nearest->n_matches;
                if (variance < 0) {
                    variance = -variance;
                }
                nearest->radius += variance;
            }
            nearest->radius = nearest->distanceAvg + sqrt(nearest->radius) * params->radiusF;
            //
            nearest->time_mu_μ = nearest->timeLinearSum / nearest->n_matches;
            nearest->time_sigma_σ = sqrt((nearest->timeSquareSum / nearest->n_matches) - (nearest->time_mu_μ * nearest->time_mu_μ));
            // assumed last m arrivals in each micro-cluster to be __m = n__
            // so, the m/(2 · n)-th percentile is the 50th percentile
            // therefore z-indez is 0.0 and using `time_relevance_stamp = μ + z*σ`
            // time_relevance_stamp is the mean when the storage feature is not used;
        } else {
            // add new pattern, for a new cluster to enter, one must leave
            // can delete?
            Cluster *oldest = NULL;
            for (size_t clId = 0; clId < params->cluStream_q_maxMicroClusters; clId++) {
                // minimize relavance stamp
                if (oldest == NULL || microClusters[clId].time_mu_μ < oldest->time_mu_μ) {
                    oldest = &microClusters[clId];
                }
            }
            Cluster *deleted = NULL;
            if (oldest->time_mu_μ < params->cluStream_time_threshold_delta_δ) {
                // is older than the parameter threshold δ. ""delete""
                deleted = oldest;
            } else {
                // can't delete any, so merge
                Cluster *merged = NULL;
                minDist = 0;
                for (size_t aId = 0; aId < params->cluStream_q_maxMicroClusters - 1; aId++) {
                    Cluster *a = &microClusters[aId];
                    for (size_t bId = aId + 1; bId < params->cluStream_q_maxMicroClusters; bId++) {
                        Cluster *b = &microClusters[bId];
                        double dist = euclideanDistance(params->dim, a->center, b->center);
                        if (merged == NULL || deleted == NULL || dist < minDist) {
                            merged = a;
                            deleted = b;
                            minDist = dist;
                        }
                    }
                }
                // merged->ids append nearB->ids
                merged->n_matches += deleted->n_matches;
                merged->timeLinearSum += deleted->timeLinearSum;
                merged->timeSquareSum += deleted->timeSquareSum;
                merged->radius = 0;
                merged->distanceAvg = 0;
                for (size_t d = 0; d < params->dim; d++) {
                    merged->ls_valLinearSum[d] += deleted->ls_valLinearSum[d];
                    merged->ss_valSquareSum[d] += deleted->ss_valSquareSum[d];
                    //
                    merged->center[d] = merged->ls_valLinearSum[d] / merged->n_matches;
                    //
                    merged->distanceAvg += merged->ls_valLinearSum[d] / merged->n_matches;
                    //
                    double variance = merged->ls_valLinearSum[d] / merged->n_matches;
                    variance *= variance;
                    variance -= merged->ss_valSquareSum[d] / merged->n_matches;
                    if (variance < 0) {
                        variance = -variance;
                    }
                    merged->radius += variance;
                }
                merged->radius = merged->distanceAvg + sqrt(merged->radius) * params->radiusF;
                //
                merged->time_mu_μ = merged->timeLinearSum / merged->n_matches;
                merged->time_sigma_σ = sqrt((merged->timeSquareSum / merged->n_matches) - (merged->time_mu_μ * merged->time_mu_μ));
            }
            nearest = NULL;
            double minDist;
            for (size_t clId = 0; clId < params->cluStream_q_maxMicroClusters; clId++) {
                double dist = euclideanDistance(params->dim, microClusters[clId].center, oldest->center);
                if (nearest == NULL || minDist > dist) {
                    minDist = dist;
                    nearest = &microClusters[clId];
                }
            }
            deleted->id = nextClusterId++;
            for (size_t d = 0; d < params->dim; d++) {
                deleted->center[d] = ex->val[d];
                deleted->ls_valLinearSum[d] = ex->val[d];
                deleted->ss_valSquareSum[d] = ex->val[d] * ex->val[d];
                deleted->radius = minDist;
                deleted->distanceAvg = 0.0;
                deleted->distanceLinearSum = 0.0;
                deleted->distanceSquareSum = 0.0;
                deleted->distanceStdDev = 0.0;
            }
        }
        // don't use any snapshot
        /*
        if (i % params->clustream_alpha == 0) {
            store away
                the current set of micro-clusters (possibly on disk) to-
                gether with their id list, and indexed by their time of
                storage. We also delete the least recent snapshot
        }
        */
    }
    fprintf(stderr, "CluStream final cluster %lu\n", nextClusterId);
    // macro clustering over all data
    Cluster *clusters = calloc(params->k, sizeof(Cluster));
    for (size_t clId = 0; clId < params->k; clId++) {
        clusters[clId].id = initalId + clId;
        clusters[clId].n_matches = 0;
        clusters[clId].center = calloc(params->dim, sizeof(double));
        clusters[clId].ls_valLinearSum = calloc(params->dim, sizeof(double));
        clusters[clId].ss_valSquareSum = calloc(params->dim, sizeof(double));
        for (size_t d = 0; d < params->dim; d++) {
            clusters[clId].center[d] = microClusters[clId].ls_valLinearSum[d] / microClusters[clId].n_matches;
            clusters[clId].ls_valLinearSum[d] = 0.0;
            clusters[clId].ss_valSquareSum[d] = 0.0;
        }
    }
    Example *pseudoExamples = calloc(params->cluStream_q_maxMicroClusters, sizeof(Example));
    for (size_t clId = 0; clId < params->cluStream_q_maxMicroClusters; clId++) {
        pseudoExamples[clId].id = clId;
        pseudoExamples[clId].val = calloc(params->dim, sizeof(double));
        for (size_t d = 0; d < params->dim; d++) {
            pseudoExamples[clId].val[d] = microClusters[clId].ls_valLinearSum[d] / microClusters[clId].n_matches;
        }
    }
    kMeans(params, clusters, pseudoExamples, params->cluStream_q_maxMicroClusters);
    printTiming(cluStream, trainingSetSize);
    return clusters;
}

#endif // !_CLUSTREAM_C
