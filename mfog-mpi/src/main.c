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
#define MAIN 1

#define TRAINING_CSV "TRAINING_CSV"
#define MODEL_CSV "MODEL_CSV"
#define EXAMPLES_CSV "EXAMPLES_CSV"
#define MATCHES_CSV "MATCHES_CSV"
#define TIMING_LOG "TIMING_LOG"

struct main_mfog_st {
    int mpiRank, mpiSize;
    char *executable;
    int kParam, dimension, isModelServer;
    char *kParamStr, *dimensionStr;
    char *trainingCsv, *modelCsv, *examplesCsv, *matchesCsv, *timingLog;
    FILE *trainingFile, *modelFile, *examplesFile, *matchesFile, *timingFile;
    SOCKET modelStore;
};

int mainClassify(struct main_mfog_st *params, Point examples[], Model *model, int *nMatches, Match *memMatches) {
    clock_t start = clock();
    double noveltyThreshold = 2;
    int minExCluster = 20;
    int maxUnkSize = params->kParam * minExCluster;
    int exampleCounter = 0;
    *nMatches = 0;
    int thresholdForgettingPast = 10000;
    // int lastCheck = 0;
    if (params->mpiSize == 1) {
        Point **unknowns = malloc(maxUnkSize * sizeof(Point *));
        size_t unknownsSize = 0;
        for (exampleCounter = 0; examples[exampleCounter].value != NULL; exampleCounter++) {
            Point *example = &(examples[exampleCounter]);
            Match *match = &(memMatches[*nMatches]);
            (*nMatches)++;
            classify(model->dimension, model, example, match);
            if (match->label == '-') {
                unknowns[unknownsSize] = example;
                unknownsSize++;
                if (unknownsSize >= maxUnkSize) {
                    //  && (lastCheck + (k * minExCluster) < exampleCounter)) {
                    // lastCheck = exampleCounter;
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
            }
            if (exampleCounter % thresholdForgettingPast == 0) {
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

#define MODEL_SERVER_PORT 7200

Model *modelStoreService(struct main_mfog_st *params) {
    Model *model;
    if (params->mpiRank != 0) {
        // otherRank
        return NULL;
    }
    if (params->trainingCsv != NULL && params->trainingFile != NULL) {
        int nExamples;
        Point *examples = readExamples(params->dimension, params->trainingFile, &nExamples, params->timingFile, params->executable);
        model = MNS_offline(nExamples, examples, params->kParam, params->dimension, params->timingFile, params->executable);
    } else if (params->modelCsv != NULL && params->modelFile != NULL) {
        model = readModel(params->dimension, params->modelFile, params->timingFile, params->executable);
    } else {
        MPI_Finalize();
        errx(EXIT_FAILURE, "No model file nor training file where provided.");
    }
    //
    int time = 0;
    char outputModelFileName[200];
    sprintf(outputModelFileName, "out/model-store/%d.csv", time);
    FILE *modelFile = fopen(outputModelFileName, "w+");
    if (modelFile == NULL)
        errx(EXIT_FAILURE, "Gib me file '%s'. At "__FILE__":%d\n", outputModelFileName, __LINE__);
    size_t modelFileSize = 0;
    modelFileSize += fprintf(modelFile, "# Model(dimension=%d, nextNovelty=%c, size=%d)\n",
                                model->dimension, model->nextNovelty, model->size);
    modelFileSize += writeModel(modelFile, model, params->timingFile, params->executable);
    printf("Model size = %10lu\n", modelFileSize);
    // int modelFd = fileno(modelFile);
    //
    server_t *server = serverStart(MODEL_SERVER_PORT);
    int bufferSize = 256;
    char *buffer = malloc((bufferSize + 1) * sizeof(char));
    int out = 0;
    while (!out) {
        printf("serverSelect\n");
        serverSelect(server);
        for (size_t i = 0; i < server->clientsLen; i++) {
            SOCKET connection = server->clients[i];
            if (!FD_ISSET(connection, &server->readfds))
                continue;
            bzero(buffer, bufferSize);
            ssize_t valread = read(connection, buffer, bufferSize - 1);
            if (valread == 0) {
                serverDisconnect(server, connection, i);
                continue;
            }
            buffer[valread] = '\0';
            printf("buffer '%s'\n", buffer);
            if (strcmp("can haz model?\n", buffer) == 0) {
                bzero(buffer, bufferSize);
                printf("Sending Model size = %10lu\n", modelFileSize);
                sprintf(buffer, "%10lu\n", modelFileSize);
                //
                if (write(connection, buffer, 11) < 0)
                    errx(EXIT_FAILURE, "ERROR writing to socket. At "__FILE__":%d\n", __LINE__);
                rewind(modelFile);
                off_t bytes_sent = sendfile(connection, dup(fileno(modelFile)), NULL, modelFileSize);
                printf("bytes_sent = %10lu\n", bytes_sent);
            }
            if (buffer[0] == 'q' && buffer[1] == '\n') {
                out = 1;
                break;
            }
        }
    }
    for (size_t i = 0; i < server->clientsLen; i++) {
        close(server->clients[i]);
    }
    close(server->serverSocket);
    free(buffer);
    free(server);
    // fclose(modelFile); // breaks, don't know why
    return model;
}

Model *getModelFromStore(struct main_mfog_st *params) {
    params->modelStore = clientConnect("127.0.0.1", MODEL_SERVER_PORT);
    if (write(params->modelStore, "can haz model?\n", 15) < 0)
        errx(EXIT_FAILURE, "ERROR writing to socket. At "__FILE__":%d\n", __LINE__);
    int bufferSize = 256;
    char *buffer = malloc((bufferSize + 1) * sizeof(char));
    //
    bzero(buffer, bufferSize);
    if (read(params->modelStore, buffer, 11) < 0)
        errx(EXIT_FAILURE, "ERROR read to socket. At "__FILE__":%d\n", __LINE__);
    printf("buffer '%s'\n", buffer);
    size_t modelFileSize = 0;
    sscanf(buffer, "%10lu", &modelFileSize);
    printf("Model size = %10lu\n", modelFileSize);
    //
    char modelFileName[200];
    int time = 0;
    sprintf(modelFileName, "out/models/remote-%d.csv", time);
    FILE *modelFile = fopen(modelFileName, "w+");
    if (modelFile == NULL)
        errx(EXIT_FAILURE, "Gib me file '%s'. At "__FILE__":%d\n", modelFileName, __LINE__);
    ssize_t totBytesRcv = 0;
    while (totBytesRcv < modelFileSize) {
        bzero(buffer, bufferSize);
        ssize_t bytes_received = read(params->modelStore, buffer, bufferSize);
        totBytesRcv += bytes_received;
        if (totBytesRcv > modelFileSize) {
            bytes_received -= modelFileSize - totBytesRcv;
            totBytesRcv = modelFileSize;
        }
        fprintf(modelFile, "%s", buffer);
    }
    printf("bytes_received = %10ld\n", totBytesRcv);
    if (totBytesRcv != modelFileSize)
        errx(EXIT_FAILURE, "ERROR read to socket. At "__FILE__":%d\n", __LINE__);
    rewind(modelFile);
    Model *model;
    model = readModel(params->dimension, modelFile, params->timingFile, params->executable);
    fclose(modelFile);
    free(buffer);
    // close(params->modelStore);
    return model;
}

void initMPI(int argc, char *argv[], char **envp, struct main_mfog_st *params) {
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
    params->mpiRank = mpiRank;
    params->mpiSize = mpiSize;
}

void initEnv(int argc, char *argv[], char **envp, struct main_mfog_st *params) {
    initMPI(argc, argv, envp, params);
    params->executable = argv[0];
    params->kParam = 100;
    params->dimension = 22;
    params->isModelServer = 0;
    params->trainingCsv = NULL;
    params->trainingFile = NULL;
    params->modelCsv = NULL;
    params->modelFile = NULL;
    params->examplesCsv = NULL;
    params->examplesFile = NULL;
    params->matchesCsv = NULL;
    params->matchesFile = NULL;
    params->timingLog = NULL;
    params->timingFile = NULL;

        int envErrors = 0;
    params->isModelServer += findEnvFlag(argc, argv, envp, "--cloud");

    params->kParamStr = findEnvVar(argc, argv, envp, "k");
    if (params->kParamStr == NULL) {
        envErrors++;
    } else {
        params->kParam = atoi(params->kParamStr);
    }
    
    params->dimensionStr = findEnvVar(argc, argv, envp, "dimension");
    if (params->dimensionStr == NULL) {
        envErrors++;
    } else {
        params->dimension = atoi(params->dimensionStr);
    }
    if (params->mpiRank != 0) {
        return;
    }
    //
    loadEnvFile(argc, argv, envp, TRAINING_CSV,   &params->trainingCsv,   &params->trainingFile,    "r");
    // envErrors += params->trainingFile == NULL;
    loadEnvFile(argc, argv, envp, MODEL_CSV,      &params->modelCsv,      &params->modelFile,       "r");
    // envErrors += params->modelFile == NULL;
    loadEnvFile(argc, argv, envp, EXAMPLES_CSV,   &params->examplesCsv,   &params->examplesFile,    "r");
    if (!params->isModelServer)
        envErrors += params->examplesFile == NULL;
    loadEnvFile(argc, argv, envp, MATCHES_CSV,    &params->matchesCsv,    &params->matchesFile,     "w");
    if (!params->isModelServer)
        envErrors += params->matchesFile == NULL;
    loadEnvFile(argc, argv, envp, TIMING_LOG,     &params->timingLog,     &params->timingFile,      "a");
    // envErrors += params->timingFile == NULL;
    //
    // printf(
    //     "isModelServer          %d\n"
    //     "Using kParam as        %d\n"
    //     "Using dimension as     %d\n"
    //     "Reading training from  (%p) '%s'\n"
    //     "Reading model from     (%p) '%s'\n"
    //     "Reading examples from  (%p) '%s'\n"
    //     "Writing matchesFile to (%p) '%s'\n"
    //     "Writing timingFile to  (%p) '%s'\n",
    //     params->isModelServer, params->kParam, params->dimension,
    //     params->trainingFile, params->trainingCsv,
    //     params->modelFile, params->modelCsv,
    //     params->examplesFile, params->examplesCsv,
    //     params->matchesFile, params->matchesCsv,
    //     params->timingFile, params->timingLog);
    fflush(stdout);
    if (envErrors != 0) {
        MPI_Finalize();
        errx(EXIT_FAILURE, "Environment errors %d. At "__FILE__":%d\n", envErrors, __LINE__);
    }
}

void sighandler(int signum) {
   printf("Caught signal %d, coming out...\n", signum);
   MPI_Finalize();
   exit(1);
}

int main(int argc, char *argv[], char **envp) {
    if (signal(SIGINT, sighandler) == SIG_ERR) {
        fputs("An error occurred while setting a signal handler.\n", stderr);
        return EXIT_FAILURE;
    }
    struct main_mfog_st params;
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
        memMatches = malloc(2 * nExamples * sizeof(Match));
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
