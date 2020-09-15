#ifndef _ND_SERVICE_C
#define _ND_SERVICE_C 1

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

#include "./minas.h"
#include "../mpi/minas-mpi.h"
#include "../util/loadenv.h"
#include "../util/net.h"

#include "./nd-service.h"

void handleUnknown(mfog_params_t *params, Model *model) {
    // model->unknowns[model->unknownsSize - 1].value = malloc(params->dimension * sizeof(double));
    int exampleCounter = model->unknowns[model->unknownsSize].id;
    if (model->unknownsSize >= params->maxUnkSize) {
        //  && (lastCheck + (k * minExCluster) < exampleCounter)) {
        // lastCheck = exampleCounter;
        // ND
        // Point *linearGroup = malloc(model->unknownsSize * sizeof(Point));
        // printf("clustering unknowns with %5ld examples\n", model->unknownsSize);
        // for (int g = 0; g < model->unknownsSize; g++) {
        //     linearGroup[g] = *model->unknowns[g];
        // }
        model = noveltyDetection(params->kParam, model, model->unknownsSize, model->unknowns,
                                 params->minExCluster, params->noveltyThreshold, params->timingFile, params->executable);
        char outputModelFileName[200];
        sprintf(outputModelFileName, "out/models/%d.csv", exampleCounter);
        FILE *outputModelFile = fopen(outputModelFileName, "w");
        if (outputModelFile != NULL) {
            writeModel(outputModelFile, model, params->timingFile, params->executable);
        }
        fclose(outputModelFile);
        //
        // Classify after model update
        // Match *memMatches = calloc(model->unknownsSize, sizeof(Match));
        // bzero(memMatches, model->unknownsSize * sizeof(Match));
        size_t prevUnknownsSize = model->unknownsSize;
        model->unknownsSize = 0;
        int currentForgetUnkThreshold = exampleCounter - params->thresholdForgettingPast;
        int forgotten = 0;
        int reprocessed = 0;

        // model->unknowns
        for (int unk = 0; unk < prevUnknownsSize; unk++) {
            Match *match = &model->memMatches[model->memMatchesSize];

            classify(model->dimension, model, &model->unknowns[unk], match);
            if (match->label != '-') {
                model->memMatchesSize++;
                reprocessed++;
                // printf("late classify %d %c\n", unkMatch.pointId, unkMatch.label);
            } else if (model->unknowns[unk].id < currentForgetUnkThreshold) {
                // compact unknowns
                model->unknowns[model->unknownsSize] = model->unknowns[unk];
                model->unknownsSize++;
            }
            // } else {
            //     forgotten++;
            // }
        }
        printf("late classify of %ld -> %ld unknowns, forgotten %d, reprocessed %d\n", prevUnknownsSize, model->unknownsSize, forgotten, reprocessed);
        fflush(stdout);
        // free(linearGroup);
        // return memMatches;
    }
    // return NULL;
}

void noveltyDetectionService(SOCKET connection, mfog_params_t *params, char *buffer, size_t maxBuff, Model *model) {
    // printf("%s "__FILE__":%d\n", __FUNCTION__, __LINE__);
    size_t buffLen = strlen(buffer);
    size_t offset = 0;
    // Ex(id=    646046, class=0, val=[3.785328e-02, 2.760000e-01, ]);\n
    // len 63
    size_t messageLen = 32 + params->dimension * 14 + 4;
    do {
        Point * unk = &model->unknowns[model->unknownsSize];
        model->unknownsSize++;
        // Point *unk = malloc(sizeof(Point));
        // unk->value = malloc(params->dimension * sizeof(double));
        if (buffLen < messageLen) {
            // message incomplete
            if (buffLen + messageLen > maxBuff) {
                // compact
                fprintf(stderr, "compact\n");
                for (size_t i = 0; i < offset; i++) {
                    buffer[i] = buffer[i + offset];
                    if (buffer[i + offset] == '\0')
                        break;
                }
                offset = 0;
            }
            fprintf(stderr, "read\n");
            read(connection, &buffer[buffLen], maxBuff - buffLen);
        }
        size_t assigned = sscanf(&buffer[offset], "Ex(id=%10d, class=%c, val=[", &unk->id, &unk->label);
        offset += 32;
        if (assigned != 2) {
            fprintf(stderr, "sscanf fail on buffer '%s'. At "__FILE__":%d\n", buffer, __LINE__);
            break;
        }
        // printf("remaining buff '%s'\n", &buffer[offset]);

        assigned = 0;
        for (size_t d = 0; d < params->dimension; d++){
            double x;
            assigned += sscanf(&buffer[offset], "%le, ", &x /* &unk.value[d] */);
            offset += 14;
        }
        if (assigned != params->dimension) {
            fprintf(stderr, "sscanf fail on buffer '%s'. At "__FILE__":%d\n", buffer, __LINE__);
            break;
        }
        assigned = sscanf(&buffer[offset], "]);\n");
        if (assigned != 0)
            fprintf(stderr, "sscanf fail on buffer '%s'. At "__FILE__":%d\n", buffer, __LINE__);
        offset += 4;
        //
        // printf("handleUnknown(params, model, &unk);\n");
        // model->unknowns[model->unknownsSize] = unknown;
        // model->unknownsSize++;
        handleUnknown(params, model);
        if (model->memMatchesSize > 0) {
            size_t matchesBufferSize = 93 + model->memMatchesSize * 67 + 2;
            char *matchesBuffer = calloc(matchesBufferSize, sizeof(char));
            size_t offs = sprintf(matchesBuffer, "%s", MATCH_CSV_HEADER);
            for (size_t i = 0; i < model->memMatchesSize; i++) {
                /*
                |#pointId,clusterLabel,clusterCategory,clusterId,clusterRadius,label,distance,secondDistance
                |       310,N,e,        10,0.000000e+00,N,0.000000e+00,0.000000e+00
                |       313,N,e,        10,0.000000e+00,N,0.000000e+00,0.000000e+00
                */
                offs += sprintf(&matchesBuffer[offs], MATCH_CSV_LINE_FORMAT, MATCH_CSV_LINE_PRINT_ARGS(model->memMatches[i]));
                if (offs > matchesBufferSize)
                    errx(EXIT_FAILURE, "Stupid fuck, more memory. At "__FILE__":%d\n\nbuffer=%s\n", __LINE__, matchesBuffer);
            }
            write(connection, matchesBuffer, offs);
            // free(matchesBuffer);
            model->memMatchesSize = 0;
        }
    } while (offset < buffLen);
}

// server_t *glob_server = NULL;

Model *modelStoreService(mfog_params_t *params) {
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
    model->unknownsSize = 0;
    model->unknowns = calloc(params->maxUnkSize, sizeof(Point));
    for (size_t i = 0; i < params->maxUnkSize; i++) {
        model->unknowns[i].value = calloc(params->dimension, sizeof(double));
    }
    
    model->memMatchesSize = 0;
    model->memMatches = calloc(params->maxUnkSize, sizeof(Match));
    // int modelFd = fileno(modelFile);
    //
    server_t *server = serverStart(MODEL_SERVER_PORT);
    // glob_server = server;
    // serverSocketFD = server->serverSocket;
    int bufferSize = 256;
    char *buffer = calloc(bufferSize + 1, sizeof(char));
    int out = 0;
    while (!out) {
        // printf("serverSelect\n");
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
            // handle request
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
            if (buffer[0] == 'E' && buffer[1] == 'x') {
                // printf("buffer '%s'\n", buffer);
                noveltyDetectionService(connection, params, buffer, bufferSize, model);
                // return model;
                // int n = write(params->modelStore, buffer, strlen(buffer));
            }
            if (buffer[0] == 'q' && buffer[1] == '\n') {
                printf("Got quit command. Bye!\n");
                out = 1;
                break;
            }
            continue;
            printf("Unknown command! buffer '%s'\n", buffer);
        }
    }
    for (size_t i = 0; i < server->clientsLen; i++) {
        close(server->clients[i]);
    }
    close(server->serverSocket);
    // free(buffer);
    // free(server);
    // fclose(modelFile); // breaks, don't know why
    return model;
}

Model *getModelFromStore(mfog_params_t *params) {
    params->modelStore = clientConnect("127.0.0.1", MODEL_SERVER_PORT);
    if (write(params->modelStore, "can haz model?\n", 15) < 0)
        errx(EXIT_FAILURE, "ERROR writing to socket. At "__FILE__":%d\n", __LINE__);
    int bufferSize = 256;
    char *buffer = calloc(bufferSize + 1, sizeof(char));
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

void sendUnk(mfog_params_t *params, Point *unk) {
    char buffer[256];
    bzero(buffer, 256);
    size_t offset = sprintf(buffer, "Ex(id=%10d, class=%c, val=[", unk->id, unk->label);
    for (size_t d = 0; d < params->dimension; d++){
        offset += sprintf(&buffer[offset], "%le, ", unk->value[d]);
    }
    offset += sprintf(&buffer[offset], "]);\n");
    int n = write(params->modelStore, buffer, strlen(buffer));
    if (n < 0)
        errx(EXIT_FAILURE, "ERROR writing to socket");
}

#endif // _ND_SERVICE_C
