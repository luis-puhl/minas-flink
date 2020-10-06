#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <err.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
// #include <unistd.h>
// pid_t getpid(void);
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <errno.h>

#ifndef MAIN
#define MAIN 1

#include "./baseline/base.h"
#include "./baseline/minas.h"
#include "./mpi/mfog-mpi.h"
// #include "./action.h"

/**
 * Experiments are based in this Minas config
 *      threshold = 2.0
 *      flagEvaluationType = 1
 *      thresholdForgettingPast = 10000
 *      numMicro = 100   // aka K in k-means
 *      flagMicroClusters = true
 *      
 *      minExCluster = 20
 *      validationCriterion = dec
*/

// /*
int mainClassify(Params *params, Example examples[], Model *model, int *nMatches, Match *memMatches) {
    clock_t start = clock();
    *nMatches = 0;
    // int lastCheck = 0;
    tradeModel(params, model);
    // Example ** unknowns = malloc(maxUnkSize * sizeof(Example *));
    // size_t unknownsSize = 0;
    int exampleBufferSize;
    char *exampleBuffer;
    double *valuePtr;
    Example *example;
    Match *match;
    if (params->mpiRank == 0) {
        exampleBufferSize = sizeof(Example) + params->dim * sizeof(double);
        exampleBuffer = malloc(exampleBufferSize);
        MPI_Bcast(&exampleBufferSize, 1, MPI_INT, MFOG_MAIN_RANK, MPI_COMM_WORLD);
    } else {
        MPI_Bcast(&exampleBufferSize, 1, MPI_INT, MFOG_MAIN_RANK, MPI_COMM_WORLD);
        exampleBuffer = malloc(exampleBufferSize);
        valuePtr = calloc(params->dim + 1, sizeof(double));
        example = calloc(1, sizeof(Example));
        match = calloc(1, sizeof(Match));
    }
    int running = 1, exampleCounter = 0, sendIt = 0, dest = 0;
    while (running) {
        if (params->mpiRank == 0) {
            if (examples[exampleCounter].val == NULL) {
                running = 0;
                break;
            }
            exampleCounter++;
            //
            example = &(examples[exampleCounter]);
            Match *match = &(memMatches[*nMatches]);
            (*nMatches)++;
            if (sendIt) {
                tradeExample(params, example, exampleBuffer, exampleBufferSize, &dest, valuePtr);
                tradeMatch(params, match, exampleBuffer, exampleBufferSize, &dest, valuePtr);
            } else {
                classify(params->dim, model, example, match);
            }
            if (match->label == '-') {
                if (params->useModelStore) {
                    sendUnk(params, example);
                } else {
                    model->unknowns[model->unknownsSize] = *example;
                    model->unknownsSize++;
                    handleUnknown(params, model);
                }
            }
        } else {
            tradeExample(params, example, exampleBuffer, exampleBufferSize, &dest, valuePtr);
            classify(params->dim, model, example, match);
            tradeMatch(params, match, exampleBuffer, exampleBufferSize, &dest, valuePtr);
        }
    }
    MPI_Barrier(MPI_COMM_WORLD);
    //
    if (params->mpiRank == 0) {
        if (params->timingFile) {
            PRINT_TIMING(params->timingFile, params->executable, params->mpiSize, start, exampleCounter);
        }
        fprintf(params->matchesFile, MATCH_CSV_HEADER);
        for (int i = 0; i < (*nMatches); i++) {
            fprintf(params->matchesFile, MATCH_CSV_LINE_FORMAT, MATCH_CSV_LINE_PRINT_ARGS(memMatches[i]));
        }
    }
    return exampleCounter;
}

void sighandler(int signum) {
   printf("Caught signal %d, coming out...\n", signum);
   MPI_Finalize();
    // if (glob_server != 0) {
    //     for (size_t i = 0; i < glob_server->clientsLen; i++) {
    //         close(glob_server->clients[i]);
    //     }
    //     close(glob_server->serverSocket);
    // }
   printf("Done signal %d\n", signum);
   exit(1);
}
// */

int main(int argc, char *argv[], char **envp) {
    clock_t start = clock();
    TimingLog timingLog;
    timingLog.len = 0;
    timingLog.maxLen = 10;
    timingLog.listHead = calloc(timingLog.maxLen, sizeof(TimingLogEntry));
    if (argc == 2) {
        fprintf(stderr, "reading from file %s\n", argv[1]);
        stdin = fopen(argv[1], "r");
    }
    Params *params = calloc(1, sizeof(Params));
    params->executable = argv[0];
    fprintf(stderr, "%s\n", params->executable);
    // getParams((*params));
    // scanf("remoteRedis" "=" "%s" "\n", params->remoteRedis);
    // params->remoteRedis = "ec2-18-191-2-174.us-east-2.compute.amazonaws.com";
    params->remoteRedis = "localhost";
    fprintf(stderr, "\t" "remoteRedis" " = " "%s" "\n", params->remoteRedis);
    //
    printTiming(main, 1);
    // return action(params);
    /*
    Model *model = NULL;
    if (params.isModelServer) {
        model = modelStoreService(&params);
        // MPI_Finalize(); // breaks, don't know why
        free(model);
        return EXIT_SUCCESS;
    }
    if (params.mpiRank == 0) {
        if (params.useModelStore) {
            model = getModelFromStore(&params);
        } else {
            model = readModel(params.dimension, params.modelFile, params.timingFile, params.executable);
        }
    }
    Example *examples = NULL;
    Match *memMatches = NULL;
    int nExamples;
    if (params.mpiRank == 0 && params.examplesCsv != NULL && params.examplesFile != NULL) {
        examples = readExamples(params->dim, params.examplesFile, &nExamples, params.timingFile, params.executable);
        // max 2 matches per example
        memMatches = calloc(2 * nExamples, sizeof(Match));
        if (!params.useModelStore) {
            model->memMatches = memMatches;
            model->unknowns = calloc(nExamples, sizeof(Example));
        }
    }
    int nMatches = 0;
    mainClassify(&params, examples, model, &nMatches, memMatches);

    if (params.mpiRank == 0) {
        char outputModelFileName[200];
        sprintf(outputModelFileName, "out/models/%d-final.csv", nExamples);
        FILE *outputModelFile = fopen(outputModelFileName, "w");
        if (outputModelFile != NULL) {
            writeModel(outputModelFile, model, params.timingFile, params.executable);
        }
        fclose(outputModelFile);
    }
    closeEnvFile(TRAINING_CSV, params.trainingCsv, params.trainingFile);
    closeEnvFile(MODEL_CSV, params.modelCsv, params.modelFile);
    closeEnvFile(EXAMPLES_CSV, params.examplesCsv, params.examplesFile);
    closeEnvFile(MATCHES_CSV, params.matchesCsv, params.matchesFile);
    closeEnvFile(TIMING_LOG, params.timingLog, params.timingFile);
    free(model);
    free(examples);
    free(memMatches);
    MPI_Finalize();
    return EXIT_SUCCESS;
    */
}

#endif // MAIN
