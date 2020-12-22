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
    MinasParams *minasParams;
    MinasState *minasState;
    char outputMode, nClassifiers;
} ThreadArgs;

int main(int argc, char const *argv[]) {
    clock_t start = clock();
    MinasParams minasParams = {
        .k=100, .dim=22, .precision=1.0e-08,
        .radiusF=0.25, .minExamplesPerCluster=20, .noveltyF=1.4,
        .thresholdForgettingPast = 10000,
    };
    minasParams.noveltyDetectionTrigger = minasParams.minExamplesPerCluster * minasParams.k;
    minasParams.unknownsMaxSize = minasParams.noveltyDetectionTrigger * 2;
    MinasState minasState = MINAS_STATE_EMPTY;
    minasState.unknowns = calloc(minasParams.unknownsMaxSize + 1, sizeof(Example));
    for (unsigned long int i = 0; i < minasParams.unknownsMaxSize + 1; i++) {
        minasState.unknowns[i].val = calloc(minasParams.dim, sizeof(double));
    }
    //
    ThreadArgs args = {
        .outputMode = argc >= 2 ? atoi(argv[1]) : MFOG_OUTPUT_ALL,
        .nClassifiers = 1, // argc >= 3 ? atoi(argv[2]) : 1,
        .minasParams = &minasParams,
        .minasState = &minasState,
    };
    //
    printArgs(minasParams, args.outputMode, args.nClassifiers);
    //
    Example example;
    example.val = calloc(minasParams.dim, sizeof(double));
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
    cluster.center = calloc(minasParams.dim, sizeof(double));
    while (!feof(stdin)) {
        clock_t t0 = clock();
        char lineType = getMfogLine(stdin, &lineptr, &n, minasParams.k, minasParams.dim, &cluster, &example);
        clock_t t1 = clock();
        ioTime += t1 - t0;
        if (lineType == 'C') {
            if (minasState.model.size <= cluster.id) {
                Cluster *cl = addCluster(minasParams.dim, &cluster, &minasState.model);
                cl->isIntrest = args.outputMode >= MFOG_OUTPUT_ALL;
                cl->latest_match_id = minasState.currId;
                clock_t t2 = clock();
                cpuTime += t2 - t1;
                if (args.outputMode >= MFOG_OUTPUT_ALL) {
                    printCluster(minasParams.dim, cl);
                    clock_t t3 = clock();
                    ioTime += t3 - t2;
                }
                if (minasState.model.size == minasParams.k) {
                    fprintf(stderr, "model complete\n");
                }
            }
        }
        if (lineType != 'E') {
            continue;
        }
        example.id = minasState.currId;
        minasState.currId++;
        // fprintf(stderr, "minasState.currId %u\n", minasState.currId);
        // minasState.currId = example.id;
        assert(minasState.model.size >= minasParams.k);
        //
        Match match;
        identify(&minasParams, &minasState.model, &example, &match);
        example.label = match.label;
        minasHandleSleep(&minasParams, &minasState);
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
            for (unsigned int d = 0; d < minasParams.dim; d++)
                printf(", %le", example.val[d]);
            printf("\n");
            fflush(stdout);
        }
        clock_t t4 = clock();
        ioTime += t4 - t3;
        //
        // unsigned int prevSize = minasState.model.size;
        ModelLink *prevTail = minasState.model.tail;
        minasHandleUnknown(&minasParams, &minasState, &example);
        clock_t t5 = clock();
        cpuTime += t5 - t4;
        if (prevTail != minasState.model.tail) {
            for (ModelLink *curr = prevTail->next; curr != NULL; curr = curr->next) {
                curr->cluster.isIntrest = args.outputMode >= MFOG_OUTPUT_MINIMAL;
                if (args.outputMode >= MFOG_OUTPUT_ALL) {
                    printCluster(minasParams.dim, &curr->cluster);
                }
                // assertMpi(MPI_Bcast(newCl, sizeof(Cluster), MPI_BYTE, MFOG_RANK_MAIN, MPI_COMM_WORLD));
                // assertMpi(MPI_Bcast(newCl->center, minasParams.dim, MPI_DOUBLE, MFOG_RANK_MAIN, MPI_COMM_WORLD));
            }
            clock_t t6 = clock();
            ioTime += t6 - t5;
        }
    }
    char *stats = calloc(minasState.model.size * 30, sizeof(char));
    restoreSleep(&minasParams, &minasState);
    fprintf(stderr, "[classifier] Statistics: %s\n", labelMatchStatistics(&minasState.model, stats));
    free(stats);
    // free(minasParams);
    free(minasState.unknowns);
    // free(minasState.model.clusters);
    // free(minasState.model);
    // free(minasState.sleep->clusters);
    // free(minasState.sleep);
    // free(minasState);
    // free(args);
    double ioTime_d = ((double)ioTime) / 1000000.0;
    double cpuTime_d = ((double)cpuTime) / 1000000.0;
    double lockTime_d = ((double)lockTime) / 1000000.0;
    double totalTime_d = ((double)clock() - start) / 1000000.0;
    double diffTime = totalTime_d - (ioTime + cpuTime + lockTime);
    fprintf(stderr, "[%s] (ioTime %le), (cpuTime %le), (lockTime %le), (total %le), (rest %le). At %s:%d\n\n",
            argv[0], ioTime_d, cpuTime_d, lockTime_d, totalTime_d, diffTime, __FILE__, __LINE__);
    return EXIT_SUCCESS;
}
