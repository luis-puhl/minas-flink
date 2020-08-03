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

#include "./minas/minas.h"
#include "./mpi/minas-mpi.h"
#include "./minas/nd-service.h"
#include "./util/loadenv.h"
#include "./util/net.h"

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

int mainClassify(mfog_params_t *params, Point examples[], Model *model, int *nMatches, Match *memMatches) {
    clock_t start = clock();
    int exampleCounter = 0;
    *nMatches = 0;
    // int lastCheck = 0;
    if (params->mpiSize == 1) {
        // Point ** unknowns = malloc(maxUnkSize * sizeof(Point *));
        // size_t unknownsSize = 0;
        for (exampleCounter = 0; examples[exampleCounter].value != NULL; exampleCounter++) {
            Point *example = &(examples[exampleCounter]);
            Match *match = &(memMatches[*nMatches]);
            (*nMatches)++;
            classify(model->dimension, model, example, match);
            if (match->label == '-') {
                sendUnk(params, example);
            /*
                unknowns[unknownsSize] = example;
                unknownsSize++;
                if (unknownsSize >= maxUnkSize) {
                    //  && (lastCheck + (k * minExCluster) < exampleCounter)) {
                    lastCheck = exampleCounter;
                    // ND
                    Point *linearGroup = malloc(unknownsSize * sizeof(Point));
                    printf("clustering unknowns with %5ld examples\n", unknownsSize);
                    for (int g = 0; g < unknownsSize; g++) {
                        linearGroup[g] = *unknowns[g];
                    }
                    model = noveltyDetection(params->kParam, model, unknownsSize, linearGroup,
                        minExCluster, noveltyThreshold, params->timingFile, params->executable);
                    char outputModelFileName[200];
                    sprintf(outputModelFileName, "out/models/%d.csv", exampleCounter);
                    FILE *outputModelFile = fopen(outputModelFileName, "w");
                    if (outputModelFile != NULL) {
                        writeModel(outputModelFile, model, params->timingFile, params->executable);
                    }
                    fclose(outputModelFile);
                    //
                    // Classify after model update
                    size_t prevUnknownsSize = unknownsSize;
                    unknownsSize = 0;
                    int currentForgetUnkThreshold = exampleCounter - thresholdForgettingPast;
                    int forgotten = 0;
                    for (int unk = 0; unk < prevUnknownsSize; unk++) {
                        match = &(memMatches[*nMatches]);
                        classify(model->dimension, model, unknowns[unk], match);
                        if (match->label != '-') {
                            (*nMatches)++;
                            // printf("late classify %d %c\n", unkMatch.pointId, unkMatch.label);
                        } else if (unknowns[unk]->id > currentForgetUnkThreshold) {
                            // compact unknowns
                            unknowns[unknownsSize] = unknowns[unk];
                            unknownsSize++;
                        } else {
                            forgotten++;
                        }
                    }
                    printf("late classify of %ld -> %ld unknowns, forgotten %d\n", prevUnknownsSize, unknownsSize, forgotten);
                    fflush(stdout);
                    free(linearGroup);
                }
            */
            }
            if (exampleCounter % params->thresholdForgettingPast == 0) {
                // put old clusters in model to sleep
            }
        }
        if (params->timingFile) {
            PRINT_TIMING(params->timingFile, params->executable, params->mpiSize, start, exampleCounter);
        }
        fprintf(params->matchesFile, MATCH_CSV_HEADER);
        for (int i = 0; i < (*nMatches); i++) {
            fprintf(params->matchesFile, MATCH_CSV_LINE_FORMAT, MATCH_CSV_LINE_PRINT_ARGS(memMatches[i]));
        }
    } else if (params->mpiRank == 0) {
        MPI_Barrier(MPI_COMM_WORLD);
        sendModel(model, params->mpiRank, params->mpiSize, params->timingFile, params->executable);
        
        MPI_Barrier(MPI_COMM_WORLD);
        int exampleCounter = sendExamples(model->dimension, examples, memMatches, params->mpiSize, params->timingFile, params->executable);

        MPI_Barrier(MPI_COMM_WORLD);
        if (params->timingFile) {
            PRINT_TIMING(params->timingFile, params->executable, params->mpiSize, start, exampleCounter);
        }
        fprintf(params->matchesFile, MATCH_CSV_HEADER);
        for (int i = 0; i < exampleCounter; i++) {
            fprintf(params->matchesFile, MATCH_CSV_LINE_FORMAT, MATCH_CSV_LINE_PRINT_ARGS(memMatches[i]));
        }
        // closeEnv(envSize, varNames, fileNames, values, fileModes);
    } else {
        MPI_Barrier(MPI_COMM_WORLD);
        model = malloc(sizeof(Model));
        receiveModel(model, params->mpiRank);
        MPI_Barrier(MPI_COMM_WORLD);

        receiveExamples(model->dimension, model, params->mpiRank);

        MPI_Barrier(MPI_COMM_WORLD);
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

int main(int argc, char *argv[], char **envp) {
    if (signal(SIGINT, sighandler) == SIG_ERR) {
        fputs("An error occurred while setting a signal handler.\n", stderr);
        return EXIT_FAILURE;
    }
    mfog_params_t params;
    //
    int mpiReturn;
    mpiReturn = MPI_Init(&argc, &argv);
    if (mpiReturn != MPI_SUCCESS) {
        MPI_Abort(MPI_COMM_WORLD, mpiReturn);
        errx(EXIT_FAILURE, "MPI Abort %d\n", mpiReturn);
    }
    int mpiRank, mpiSize;
    mpiReturn = MPI_Comm_size(MPI_COMM_WORLD, &mpiSize);
    mpiReturn = MPI_Comm_rank(MPI_COMM_WORLD, &mpiRank);
    if (mpiReturn != MPI_SUCCESS) {
        MPI_Abort(MPI_COMM_WORLD, mpiReturn);
        errx(EXIT_FAILURE, "MPI Abort %d\n", mpiReturn);
    }
    printf("MPI rank / size => %d/%d\n", mpiRank, mpiSize);
    params.mpiRank = mpiRank;
    params.mpiSize = mpiSize;
    //
    initEnv(argc, argv, envp, &params);
    //
    Model *model;
    if (params.isModelServer) {
        model = modelStoreService(&params);
        // MPI_Finalize(); // breaks, don't know why
        free(model);
        return 0;
    }
    model = getModelFromStore(&params);
    Point *examples;
    Match *memMatches;
    int nExamples;
    if (params.examplesCsv != NULL && params.examplesFile != NULL) {
        examples = readExamples(model->dimension, params.examplesFile, &nExamples, params.timingFile, params.executable);
        // max 2 matches per example
        memMatches = calloc(2 * nExamples, sizeof(Match));
    }
    int nMatches;
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
    return 0;
}

#endif // MAIN
