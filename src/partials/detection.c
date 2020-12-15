#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <math.h>
#include <time.h>
#include <ctype.h>

#include "../base.h"

int main(int argc, char const *argv[]) {
    clock_t start = clock();
    unsigned int kParam = 100, dim = 22, minExamplesPerCluster = 20, thresholdForgettingPast = 10000;
    double precision = 1.0e-08, radiusF = 0.25, noveltyF = 1.4;
    fprintf(stderr, "%s; kParam=%u; dim=%u; precision=%le; radiusF=%le; minExamplesPerCluster=%u; noveltyF=%le, thresholdForgettingPast=%u\n", argv[0], PARAMS);
    Model *model = calloc(1, sizeof(Model));
    model->size = 0;
    model->nextLabel = 0;
    model->clusters = calloc(kParam, sizeof(Cluster));
    // Match match;
    Example example;
    double valuePtr[dim];
    example.val = valuePtr;
    Cluster cluster;
    cluster.center = calloc(dim, sizeof(double));
    Match match;
    //
    unsigned long int noveltyDetectionTrigger = minExamplesPerCluster * kParam;
    unsigned long int unknownsMaxSize = noveltyDetectionTrigger * 2;
    Example *unknowns = calloc(unknownsMaxSize + 1, sizeof(Example));
    for (unsigned long int i = 0; i < unknownsMaxSize + 1; i++) {
        unknowns[i].val = calloc(dim, sizeof(double));
    }
    unsigned long int unknownsSize = 0, lastNDCheck = 0, id = 0;
    fprintf(stderr, "Taking unknown stream from stdin\n");
    //
    char *lineptr = NULL;
    size_t n = 0, inputLine = 0;
    unsigned long int globGarbageCollected = 0, globConsumed = 0, globReclassified = 0;
    while (!feof(stdin)) {
        char lineType = getMfogLine(stdin, &lineptr, &n, kParam, dim, &id, model, &cluster, &example);
        inputLine++;
        if (lineType == 'C') {
            if (model->size <= cluster.id) {
                addCluster(kParam, dim, &cluster, model);
                printCluster(dim, &cluster);
                if (model->size == kParam) {
                    fprintf(stderr, "[%s] model complete\n", argv[0]);
                }
            }
        }
        if (lineType != 'U') {
            continue;
        }
        if (model->size >= kParam) {
            identify(kParam, dim, precision, radiusF, model, &example, &match, thresholdForgettingPast);
            if (match.isMatch) {
                printf("%10u,%s\n", example.id, printableLabel(match.label));
                fflush(stdout);
                globReclassified++;
                continue;
            }
        }
        //
        unknowns[unknownsSize].id = example.id;
        unknowns[unknownsSize].label = example.label;
        for (size_t i = 0; i < dim; i++) {
            unknowns[unknownsSize].val[i] = example.val[i];
        }
        unknownsSize++;
        if (unknownsSize >= unknownsMaxSize) {
            unsigned long int garbageCollected = 0;
            for (unsigned long int ex = 0; ex < unknownsSize; ex++) {
                // compress
                unknowns[ex - garbageCollected] = unknowns[ex];
                if (unknowns[ex].id < lastNDCheck) {
                    garbageCollected++;
                    continue;
                }
            }
            globGarbageCollected += garbageCollected;
            unknownsSize -= garbageCollected;
            fprintf(stderr, "[detector] garbageCollect unknowns to %lu "__FILE__":%d\n", garbageCollected, __LINE__);
        }
        if (model->size < kParam) {
            continue;
        }
        assert(unknownsSize < unknownsMaxSize);
        //
        if (unknownsSize >= noveltyDetectionTrigger && id - lastNDCheck > noveltyDetectionTrigger) {
            unsigned int prevSize = model->size, noveltyCount;
            unsigned int nNewClusters = noveltyDetection(PARAMS, model, unknowns, unknownsSize, &noveltyCount);
            //
            for (unsigned long int k = prevSize; k < model->size; k++) {
                Cluster *newCl = &model->clusters[k];
                newCl->isIntrest = 1;
                newCl->n_matches = 0;
                newCl->n_misses = 0;
                printCluster(dim, newCl);
            }
            //
            unsigned long int garbageCollected = 0, consumed = 0, reclassified = 0;
            for (unsigned long int ex = 0; ex < unknownsSize; ex++) {
                // compress
                unknowns[ex - (garbageCollected + consumed + reclassified)] = unknowns[ex];
                Cluster *nearest;
                double distance = nearestClusterVal(dim, &model->clusters[prevSize], nNewClusters, unknowns[ex].val, &nearest);
                assert(nearest != NULL);
                if (distance <= nearest->distanceMax) {
                    consumed++;
                    continue;
                }
                distance = nearestClusterVal(dim, model->clusters, model->size - nNewClusters, unknowns[ex].val, &nearest);
                assert(nearest != NULL);
                if (distance <= nearest->distanceMax) {
                    printf("%10u,%s\n", example.id, printableLabel(match.label));
                    fflush(stdout);
                    nearest->n_matches++;
                    reclassified++;
                    continue;
                }
                if (unknowns[ex].id < lastNDCheck) {
                    garbageCollected++;
                    continue;
                }
            }
            globGarbageCollected += garbageCollected;
            globConsumed += consumed;
            globReclassified += reclassified;
            unknownsSize -= (garbageCollected + consumed + reclassified);
            fprintf(stderr, "Novelties %3u, Extensions %3u, consumed %6lu, reclassified %6lu, garbageCollected %6lu\n",
                    noveltyCount, nNewClusters - noveltyCount, consumed, reclassified, garbageCollected);
            lastNDCheck = id;
        }
    }
    char *stats = calloc(model->size * 30, sizeof(char));
    fprintf(stderr, "[%s] Statistics %s\n", argv[0], labelMatchStatistics(model, stats));
    fprintf(stderr, "[%s] Global lines %lu, consumed %6lu, reclassified %6lu, garbageCollected %6lu\n", argv[0], inputLine, globConsumed, globReclassified, globGarbageCollected);
    free(stats);
    free(model);
    fflush(stdout);
    fprintf(stderr, "[%s] %le seconds. At %s:%d\n", argv[0], ((double)clock() - start) / 1000000.0, __FILE__, __LINE__);
    return EXIT_SUCCESS;
}
