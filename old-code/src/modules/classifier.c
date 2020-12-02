#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// #include <err.h>
// #include <math.h>
#include <time.h>
// #include <ctype.h>

// #include <unistd.h>
// #include <sys/types.h>
// #include <sys/socket.h>
// #include <sys/select.h>
// #include <netinet/in.h>
// #include <netdb.h>
// #include <arpa/inet.h>
// #include <poll.h>

#include <hiredis/hiredis.h>
#include <mpi.h>

#define MAIN

#include "../base/base.h"
#include "../base/minas.h"
// #include "../base/kmeans.h"
// #include "../base/clustream.h"
#include "../mpi/mfog-mpi.h"

#include "./modules.h"
#include "./redis/redis-connect.h"
#include "./redis/mfog-redis-comm.h"

int handleUnknownInline(Params *params, Example *example, Model *model,
                        size_t *unknownsMaxSize, size_t noveltyDetectionTrigger, size_t *unknownsSize,
                        size_t *classified, size_t *lastNDCheck, Example **unknowns)
{
    (*unknowns)[*unknownsSize] = *example;
    (*unknowns)[*unknownsSize].val = calloc(params->dim, sizeof(double));
    for (size_t d = 0; d < params->dim; d++) {
        (*unknowns)[*unknownsSize].val[d] = example->val[d];
    }
    (*unknownsSize)++;
    if (*unknownsSize >= *unknownsMaxSize) {
        *unknownsMaxSize *= 1.2;
        (*unknowns) = realloc(*unknowns, *unknownsMaxSize * sizeof(Example));
    }
    //
    if (*unknownsSize % noveltyDetectionTrigger == 0 && example->id - (*lastNDCheck) > noveltyDetectionTrigger) {
        (*lastNDCheck) = example->id;
        unsigned int prevSize = model->size;
        noveltyDetection(params, model, *unknowns, *unknownsSize);
        unsigned int nNewClusters = model->size - prevSize;
        //
        size_t reclassified = 0;
        for (size_t ex = 0; ex < *unknownsSize; ex++) {
            // compress
            (*unknowns)[ex - reclassified] = (*unknowns)[ex];
            // identify(params, model, &(*unknowns)[ex], &match);
            // match.label = UNK_LABEL;
            // use only new clusters
            Cluster *nearest;
            double distance = nearestClusterVal(params, &model->clusters[prevSize], nNewClusters, (*unknowns)[ex].val, &nearest);
            // match.distance = nearestClusterVal(params, model->clusters, model->size, (*unknowns)[ex].val, &match.cluster);
            assert(nearest != NULL);
            if (distance <= nearest->distanceMax) {
                printf("%10u,%s\n", (*unknowns)[ex].id, printableLabel(nearest->label));
                reclassified++;
            }
            // if (match.label == UNK_LABEL)
            //     continue;
            // printf("%10u,%s\n", (*unknowns)[ex].id, printableLabel(match.label));
            // reclassified++;
        }
        fprintf(stderr, "Reclassified %lu\n", reclassified);
        *unknownsSize -= reclassified;
    }
    return *unknownsSize;
}

int classifier(Params *params, redisContext *redisCtx, Model *model, char *buffer, size_t maxBuffSize) {
    clock_t start = clock();
    int exampleBufferSize, dest = 0;
    char *exampleBuffer;
    double *valuePtr;
    Match match;
    //
    if (params->mpiSize > 0) {
        if (params->mpiRank == 0) {
            exampleBufferSize = sizeof(Example) + params->dim * sizeof(double);
            exampleBuffer = malloc(exampleBufferSize);
            MPI_Bcast(&exampleBufferSize, 1, MPI_INT, MFOG_MAIN_RANK, MPI_COMM_WORLD);
        } else {
            MPI_Bcast(&exampleBufferSize, 1, MPI_INT, MFOG_MAIN_RANK, MPI_COMM_WORLD);
            exampleBuffer = malloc(exampleBufferSize);
            valuePtr = calloc(params->dim + 1, sizeof(double));
            // example = calloc(1, sizeof(Example));
            // match = calloc(1, sizeof(Match));
        }
        tradeModel(params, model);
    }
    //
    size_t unknownsMaxSize, noveltyDetectionTrigger, unknownsSize = 0, classified = 0, lastNDCheck = 0;
    Example *unknowns;
    if (params->useInlineND) {
        unknownsMaxSize = params->minExamplesPerCluster * params->k;
        noveltyDetectionTrigger = params->minExamplesPerCluster * params->k;
        unknowns = calloc(unknownsMaxSize, sizeof(Example));
    }
    //
    unsigned int unknownsCounter = 0;
    if (params->mpiRank == MFOG_MAIN_RANK) {
        printf("#pointId,label\n");
        //
        Example *example;
        // select(example, model)
        while (next(params, &example) != NULL) {
            if (dest == MFOG_MAIN_RANK) {
                identify(params, model, example, &match);
                classified++;
            } else {
                tradeExample(params, example, exampleBuffer, exampleBufferSize, &dest, valuePtr);
                classified++;
                tradeMatch(params, &match, exampleBuffer, exampleBufferSize, &dest, valuePtr);
                dest = (dest + 1) % params->mpiSize;
            }
            printf("%10u,%s\n", example->id, printableLabel(match.label));
            if (params->useRedis) {
                modelStoreUpdateRedis(params, redisCtx, model);
            }
            //
            if (match.label != UNK_LABEL) continue;
            // send to novelty detection service
            if (params->useRedis) {
                sendUnknownRedis(params, redisCtx, example, buffer, maxBuffSize);
            }
            if (params->useInlineND) {
                handleUnknownInline(params, example, model, &unknownsMaxSize, noveltyDetectionTrigger, &unknownsSize, &classified, &lastNDCheck, &unknowns);
            }
            unknownsCounter++;
        }
    } else if (params->mpiSize > 0) {
        Example example;
        tradeExample(params, &example, exampleBuffer, exampleBufferSize, &dest, valuePtr);
        identify(params, model, &example, &match);
        tradeMatch(params, &match, exampleBuffer, exampleBufferSize, &dest, valuePtr);
        classified++;
    }
    printTiming(classifier, classified);
    fprintf(stderr, "unknowns = %u\n", unknownsCounter);
    return classified;
}

// #ifndef MAIN
// #define MAIN 1
int main(int argc, char const *argv[], char *env[]) {
    clock_t start = clock();
    Params *params = setup(argc, argv, env);
    //
    Model *model = calloc(1, sizeof(Model));
    model->size = 0;
    model->clusters = calloc(params->k, sizeof(Cluster));
    //
    int maxBuffSize = 1024;
    char *buffer = calloc(maxBuffSize, sizeof(char));
    //
    redisContext *redisCtx;
    if (params->mpiRank == MFOG_MAIN_RANK) {
        if (params->useRedis) {
            redisCtx = makeConnection(params, model);
        } else if (params->useInlineND) {
            fprintf(stderr, "useInlineND");
            int emptyLine = 0;
            char *line = NULL;
            size_t len = 0;
            ssize_t read;
            while ((read = getline(&line, &len, stdin)) != -1 && emptyLine < 1) {
                printf("Retrieved line of length %zu :\n", read);
                printf("%s", line);
                appendClusterFromStore(params, line, read, model);
            }
        }
    }
    //
    classifier(params, redisCtx, model, buffer, maxBuffSize);
    //
    tearDown(argc, argv, env, params);
    printTiming(main, 1);
    printTimeLog(params);
    return EXIT_SUCCESS;
}
// #endif // MAIN
