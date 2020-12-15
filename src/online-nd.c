#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <errno.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>

#include "./base.h"

#define MFOG_OUTPUT_NONE    0
#define MFOG_OUTPUT_MINIMAL 1
#define MFOG_OUTPUT_ALL     2

typedef struct {
    unsigned int kParam, dim, minExamplesPerCluster, thresholdForgettingPast;
    double precision, radiusF, noveltyF;
    Model *model;
    unsigned long noveltyDetectionTrigger, unknownsMaxSize, unknownsSize, lastNDCheck, currId;
    Example *unknowns;
    char outputMode, nClassifiers;
} ThreadArgs;

int main(int argc, char const *argv[]) {
    clock_t start = clock();
    ThreadArgs args = {
        .kParam=100, .dim=22, .precision=1.0e-08,
        .radiusF=0.25, .minExamplesPerCluster=20, .noveltyF=1.4,
        .outputMode = argc >= 2 ? atoi(argv[1]) : MFOG_OUTPUT_ALL,
        .nClassifiers = argc >= 3 ? atoi(argv[2]) : 1,
        .unknownsSize = 0, .lastNDCheck = 0, .currId = 0,
        .thresholdForgettingPast = 10000,
    };
    args.model = calloc(1, sizeof(Model));
    args.model->size = 0;
    args.model->nextLabel = '\0';
    args.model->clusters = calloc(args.kParam, sizeof(Cluster));
    //
    args.noveltyDetectionTrigger = args.minExamplesPerCluster * args.kParam;
    args.unknownsMaxSize = args.noveltyDetectionTrigger * 2;
    args.unknowns = calloc(args.unknownsMaxSize + 1, sizeof(Example));
    for (unsigned long int i = 0; i < args.unknownsMaxSize + 1; i++) {
        args.unknowns[i].val = calloc(args.dim, sizeof(double));
    }
    fprintf(stderr, "%s; kParam=%d; dim=%d; precision=%le; radiusF=%le; minExamplesPerCluster=%d; noveltyF=%le;\n"
                "\toutputMode %d, nClassifiers %d\n",
                argv[0],
                args.kParam, args.dim, args.precision, args.radiusF, args.minExamplesPerCluster, args.noveltyF,
                args.outputMode, args.nClassifiers);
    //
    Example example;
    example.val = calloc(args.dim, sizeof(double));
    //
    char label[20];
    clock_t ioTime = 0, cpuTime = 0, lockTime = 0;
    if (args.outputMode >= MFOG_OUTPUT_MINIMAL) {
        printf("#pointId,label\n");
        fflush(stdout);
    }
    char *lineptr = NULL;
    size_t n = 0;
    Cluster cluster;
    cluster.center = calloc(args.dim, sizeof(double));
    while (!feof(stdin)) {
        clock_t t0 = clock();
        char lineType = getMfogLine(stdin, &lineptr, &n, args.kParam, args.dim, &args.currId, args.model, &cluster, &example);
        clock_t t1 = clock();
        ioTime += t1 - t0;
        if (lineType == 'C') {
            if (args.model->size < cluster.id) {
                Cluster *cl = addCluster(args.kParam, args.dim, &cluster, args.model);
                cl->isIntrest = args.outputMode >= MFOG_OUTPUT_ALL;
                clock_t t2 = clock();
                cpuTime += t2 - t1;
                if (args.outputMode >= MFOG_OUTPUT_ALL) {
                    printCluster(args.dim, cl);
                    clock_t t3 = clock();
                    ioTime += t3 - t2;
                }
                if (args.model->size == args.kParam) {
                    fprintf(stderr, "model complete\n");
                }
            }
        }
        if (lineType != 'E') {
            continue;
        }
        //
        Match match;
        identify(args.kParam, args.dim, args.precision, args.radiusF, args.model, &example, &match, args.thresholdForgettingPast);
        example.label = match.label;
        clock_t t2 = clock();
        cpuTime += t2 - t1;
        //
        if (args.outputMode >= MFOG_OUTPUT_MINIMAL) {
            printf("%10u,%s\n", example.id, printableLabelReuse(example.label, label));
            fflush(stdout);
        }
        //
        clock_t t3 = clock();
        ioTime += t3 - t2;
        if (example.label != MINAS_UNK_LABEL) {
            continue;
        }
        if (args.outputMode >= MFOG_OUTPUT_ALL) {
            printf("Unknown: %10u", example.id);
            for (unsigned int d = 0; d < args.dim; d++)
                printf(", %le", example.val[d]);
            printf("\n");
            fflush(stdout);
        }
        clock_t t4 = clock();
        ioTime += t4 - t3;
        //
        double *sw = args.unknowns[args.unknownsSize].val;
        args.unknowns[args.unknownsSize] = example;
        example.val = sw;
        args.unknownsSize++;
        if (args.unknownsSize >= args.unknownsMaxSize) {
            unsigned long int garbageCollected = 0;
            for (unsigned long int ex = 0; ex < args.unknownsSize; ex++) {
                // compress
                args.unknowns[ex - garbageCollected] = args.unknowns[ex];
                if (args.unknowns[ex].id < args.lastNDCheck) {
                    garbageCollected++;
                    continue;
                }
            }
            args.unknownsSize -= garbageCollected;
            clock_t t5 = clock();
            cpuTime += t5 - t4;
            fprintf(stderr, "[detector] garbageCollect unknowns to %lu "__FILE__":%d\n", garbageCollected, __LINE__);
        }
        assert(args.unknownsSize < args.unknownsMaxSize);
        //
        if (args.unknownsSize >= args.noveltyDetectionTrigger && args.currId - args.lastNDCheck > args.noveltyDetectionTrigger) {
            unsigned int prevSize = args.model->size, noveltyCount;
            unsigned int nNewClusters = noveltyDetection(args.kParam, args.dim, args.precision, args.radiusF, args.thresholdForgettingPast , args.minExamplesPerCluster, args.noveltyF, args.model, args.unknowns, args.unknownsSize, &noveltyCount);
            clock_t t3 = clock();
            cpuTime += t3 - t2;
            //
            for (unsigned long int k = prevSize; k < args.model->size; k++) {
                Cluster *newCl = &args.model->clusters[k];
                newCl->isIntrest = args.outputMode >= MFOG_OUTPUT_MINIMAL;
                if (args.outputMode >= MFOG_OUTPUT_ALL)
                    printCluster(args.dim, newCl);
            }
            clock_t t4 = clock();
            ioTime += t4 - t3;
            //
            unsigned long int garbageCollected = 0, consumed = 0, reclassified = 0;
            for (unsigned long int ex = 0; ex < args.unknownsSize; ex++) {
                // compress
                args.unknowns[ex - (garbageCollected + consumed + reclassified)] = args.unknowns[ex];
                Cluster *nearest;
                double distance = nearestClusterVal(args.dim, &args.model->clusters[prevSize], nNewClusters, args.unknowns[ex].val, &nearest);
                assert(nearest != NULL);
                if (distance <= nearest->distanceMax) {
                    consumed++;
                    continue;
                }
                distance = nearestClusterVal(args.dim, args.model->clusters, args.model->size - nNewClusters, args.unknowns[ex].val, &nearest);
                assert(nearest != NULL);
                if (distance <= nearest->distanceMax) {
                    reclassified++;
                    continue;
                }
                if (args.unknowns[ex].id < args.lastNDCheck) {
                    garbageCollected++;
                    continue;
                }
            }
            clock_t t5 = clock();
            cpuTime += t5 - t4;
            args.unknownsSize -= (garbageCollected + consumed + reclassified);
            fprintf(stderr, "Novelties %3u, Extensions %3u, consumed %6lu, reclassified %6lu, garbageCollected %6lu\n",
                    noveltyCount, nNewClusters - noveltyCount, consumed, reclassified, garbageCollected);
            args.lastNDCheck = args.currId;
        }
    }
    char *stats = calloc(args.model->size * 30, sizeof(char));
    fprintf(stderr, "[classifier] Statistics: %s\n", labelMatchStatistics(args.model, stats));
    free(stats);
    free(args.model);
    double ioTime_d = ((double)ioTime) / 1000000.0;
    double cpuTime_d = ((double)cpuTime) / 1000000.0;
    double lockTime_d = ((double)lockTime) / 1000000.0;
    double totalTime_d = ((double)clock() - start) / 1000000.0;
    double diffTime = totalTime_d - (ioTime + cpuTime + lockTime);
    fprintf(stderr, "[%s] (ioTime %le), (cpuTime %le), (lockTime %le), (total %le), (rest %le). At %s:%d\n\n",
            argv[0], ioTime_d, cpuTime_d, lockTime_d, totalTime_d, diffTime, __FILE__, __LINE__);
    return EXIT_SUCCESS;
}
