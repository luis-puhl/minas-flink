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

#include "./minas/minas.h"
#include "./mpi/minas-mpi.h"
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

#ifndef MAIN
#define MAIN
struct main_mfog_st {
    int mpiRank, mpiSize;
    char *executable;
    int kParam, dimension, isModelServer;
    char *kParamStr, *dimensionStr;
    char *trainingCsv, *modelCsv, *examplesCsv, *matchesCsv, *timingLog;
    FILE *trainingFile, *modelFile, *examplesFile, *matchesFile, *timingFile;
};

int mainClassify(int k, int mpiRank, int mpiSize, Point examples[], Model *model,
    int *nMatches, Match *memMatches, FILE *matchesFile, FILE *timingFile, char *executable) {
    clock_t start = clock();
    double noveltyThreshold = 2;
    int minExCluster = 20;
    int maxUnkSize = k * minExCluster;
    int exampleCounter = 0;
    *nMatches = 0;
    int thresholdForgettingPast = 10000;
    int lastCheck = 0;
    if (mpiSize == 1) {
        Point **unknowns = malloc(maxUnkSize * sizeof(Point *));
        int unknownsSize = 0;
        for (exampleCounter = 0; examples[exampleCounter].value != NULL; exampleCounter++) {
            Point *example = &(examples[exampleCounter]);
            Match *match = &(memMatches[*nMatches]);
            (*nMatches)++;
            classify(model->dimension, model, example, match);
            if (match->label == '-') {
                unknowns[unknownsSize] = example;
                unknownsSize++;
                if (unknownsSize >= maxUnkSize && (lastCheck + (k * minExCluster) < exampleCounter)) {
                    lastCheck = exampleCounter;
                    // ND
                    Point *linearGroup = malloc(unknownsSize * sizeof(Point));
                    printf("clustering unknowns with %5d examples\n", unknownsSize);
                    for (int g = 0; g < unknownsSize; g++) {
                        linearGroup[g] = *unknowns[g];
                    }
                    model = noveltyDetection(k, model, unknownsSize, linearGroup, minExCluster, noveltyThreshold, timingFile, executable);
                    char outputModelFileName[200];
                    sprintf(outputModelFileName, "out/models/%d.csv", exampleCounter);
                    FILE *outputModelFile = fopen(outputModelFileName, "w");
                    if (outputModelFile != NULL) {
                        writeModel(outputModelFile, model, timingFile, executable);
                    }
                    fclose(outputModelFile);
                    //
                    // Classify after model update
                    int prevUnknownsSize = unknownsSize;
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
                    printf("late classify of %d -> %d unknowns, forgotten %d\n", prevUnknownsSize, unknownsSize, forgotten);
                    fflush(stdout);
                    free(linearGroup);
                }
            }
            if (exampleCounter % thresholdForgettingPast == 0) {
                // put old clusters in model to sleep
            }
        }
        if (timingFile) {
            PRINT_TIMING(timingFile, executable, mpiSize, start, exampleCounter);
        }
        fprintf(matchesFile, MATCH_CSV_HEADER);
        for (int i = 0; i < (*nMatches); i++) {
            fprintf(matchesFile, MATCH_CSV_LINE_FORMAT, MATCH_CSV_LINE_PRINT_ARGS(memMatches[i]));
        }
    } else if (mpiRank == 0) {
        MPI_Barrier(MPI_COMM_WORLD);
        sendModel(model->dimension, model, mpiRank, mpiSize, timingFile, executable);
        
        MPI_Barrier(MPI_COMM_WORLD);
        int exampleCounter = sendExamples(model->dimension, examples, memMatches, mpiSize, timingFile, executable);

        MPI_Barrier(MPI_COMM_WORLD);
        if (timingFile) {
            PRINT_TIMING(timingFile, executable, mpiSize, start, exampleCounter);
        }
        fprintf(matchesFile, MATCH_CSV_HEADER);
        for (int i = 0; i < exampleCounter; i++) {
            fprintf(matchesFile, MATCH_CSV_LINE_FORMAT, MATCH_CSV_LINE_PRINT_ARGS(memMatches[i]));
        }
        // closeEnv(envSize, varNames, fileNames, values, fileModes);
    } else {
        MPI_Barrier(MPI_COMM_WORLD);
        model = malloc(sizeof(Model));
        receiveModel(0, model, mpiRank);
        MPI_Barrier(MPI_COMM_WORLD);

        receiveExamples(model->dimension, model, mpiRank);

        MPI_Barrier(MPI_COMM_WORLD);
    }
    return exampleCounter;
}

Model *createModelServerModule(struct main_mfog_st params) {
    Model *model;
    if (params.mpiRank == 0) {
        if (params.trainingCsv != NULL && params.trainingFile != NULL) {
            int nExamples;
            Point *examples = readExamples(params.dimension, params.trainingFile, &nExamples, params.timingFile, params.executable);
            model = MNS_offline(nExamples, examples, params.kParam, params.dimension, params.timingFile, params.executable);
            fflush(stdout);
            FILE *outputModelFile = fopen("out/models/0-initial.csv", "w");
            if (outputModelFile != NULL) {
                writeModel(outputModelFile, model, params.timingFile, params.executable);
            }
            fclose(outputModelFile);
        } else if (params.modelCsv != NULL && params.modelFile != NULL) {
            model = readModel(params.dimension, params.modelFile, params.timingFile, params.executable);
        }
    } else {
        // otherRank
    }
    return model;
}

int main(int argc, char *argv[], char **envp) {
    int isModelServer = 0 ;
    isModelServer += findEnvFlag(argc, argv, envp, "-cloud");
    isModelServer += findEnvFlag(argc, argv, envp, "--cloud");
    if (isModelServer) {
        // model = createModelServerModule(params);
        // 
        int sockfd = serverStart(7200);
        int newsockfd = serverAccept(sockfd);
        serverRead(newsockfd);
        close(newsockfd);
        close(sockfd);
    } else {
        // model = getModelFromStore(params);
        //
        printf("connect\n");
        fflush(stdout);
        int sockfd = clientConnect("127.0.0.1", 7200);
        clientRead(sockfd);
    }
    return 0;

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
    // printEnvs(argc, argv, envp);
    struct main_mfog_st params;
    params.executable = argv[0];
    params.kParam = 100;
    params.dimension = 22;
    params.isModelServer = 0;
    if (mpiRank == 0) {
        int envErrors = 0;
        params.isModelServer += findEnvFlag(argc, argv, envp, "-cloud");
        params.isModelServer += findEnvFlag(argc, argv, envp, "--cloud");
        params.kParamStr = findEnvVar(argc, argv, envp, "k");
        if (params.kParamStr == NULL) {
            envErrors++;
        } else {
            params.kParam = atoi(params.kParamStr);
        }
        params.dimensionStr = findEnvVar(argc, argv, envp, "dimension");
        if (params.dimensionStr == NULL) {
            envErrors++;
        } else {
            params.dimension = atoi(params.dimensionStr);
        }
        //
        loadEnvFile(argc, argv, envp, "TRAINING_CSV",   &params.trainingCsv,   &params.trainingFile,    "r");
        // envErrors += params.trainingFile == NULL;
        loadEnvFile(argc, argv, envp, "MODEL_CSV",      &params.modelCsv,      &params.modelFile,       "r");
        envErrors += params.modelFile == NULL;
        loadEnvFile(argc, argv, envp, "EXAMPLES_CSV",   &params.examplesCsv,   &params.examplesFile,    "r");
        envErrors += params.examplesFile == NULL;
        loadEnvFile(argc, argv, envp, "MATCHES_CSV",    &params.matchesCsv,    &params.matchesFile,     "w");
        envErrors += params.matchesFile == NULL;
        loadEnvFile(argc, argv, envp, "TIMING_LOG",     &params.timingLog,     &params.timingFile,      "a");
        envErrors += params.timingFile == NULL;
        printf(
            "isModelServer          %d\n"
            "Using kParam as        %d\n"
            "Using dimension as     %d\n"
            "Reading training from  (%p) '%s'\n"
            "Reading model from     (%p) '%s'\n"
            "Reading examples from  (%p) '%s'\n"
            "Writing matchesFile to (%p) '%s'\n"
            "Writing timingFile to  (%p) '%s'\n",
            params.isModelServer, params.kParam, params.dimension,
            params.trainingFile, params.trainingCsv,
            params.modelFile, params.modelCsv,
            params.examplesFile, params.examplesCsv,
            params.matchesFile, params.matchesCsv,
            params.timingFile, params.timingLog);
        fflush(stdout);
        if (envErrors != 0) {
            MPI_Finalize();
            errx(EXIT_FAILURE, "Environment errors %d. At "__FILE__":%d\n", envErrors, __LINE__);
            return 1;
        }
    }
    //
    Model *model;
    if (params.isModelServer) {
        // model = createModelServerModule(params);
        // 
        int sockfd = serverStart(7200);
        int newsockfd = serverAccept(sockfd);
        serverRead(newsockfd);
        close(newsockfd);
        close(sockfd);
    } else {
        // model = getModelFromStore(params);
        //
        printf("connect\n");
        fflush(stdout);
        int sockfd = clientConnect("127.0.0.1", 7200);
        clientRead(sockfd);
    }
    /*
    Point *examples;
    Match *memMatches;
    int nExamples;
    if (params.examplesCsv != NULL && params.examplesFile != NULL) {
        examples = readExamples(model->dimension, params.examplesFile, &nExamples, params.timingFile, params.executable);
        // max 2 matchesFile per example
        memMatches = malloc(2 * nExamples * sizeof(Match));
    }
    int nMatches;
    mainClassify(params.kParam, mpiRank, mpiSize, examples, model, &nMatches, memMatches, params.matchesFile, params.timingFile, params.executable);

    if (mpiRank == 0) {
        char outputModelFileName[200];
        sprintf(outputModelFileName, "out/models/%d-final.csv", nExamples);
        FILE *outputModelFile = fopen(outputModelFileName, "w");
        if (outputModelFile != NULL) {
            writeModel(outputModelFile, model, params.timingFile, params.executable);
        }
        fclose(outputModelFile);
        // closeEnv(envType, varNames, fileNames, values, fileModes);
        closeEnvFile("TRAINING_CSV", params.trainingCsv, params.trainingFile);
        closeEnvFile("MODEL_CSV", params.modelCsv, params.modelFile);
        closeEnvFile("EXAMPLES_CSV", params.examplesCsv, params.examplesFile);
        closeEnvFile("MATCHES_CSV", params.matchesCsv, params.matchesFile);
        closeEnvFile("TIMING_LOG", params.timingLog, params.timingFile);
    }
    */
    MPI_Finalize();
    // free(model);
    // free(examples);
    // free(memMatches);
    return 0;
}

#endif // MAIN
