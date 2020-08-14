#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <math.h>
#include <time.h>
#include <ctype.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <poll.h>

#define MAIN

#include "../baseline/base.h"
#include "../baseline/kmeans.h"
#include "../baseline/clustream.h"
#include "../baseline/minas.h"

#include "../util/net.h"

#include "./modules.h"

int appendClusterFromStore(Params *params, SOCKET modelStore, char *buffer, size_t buffSize, Model *model) {
    // printf("cluster line (%ld) \t'%s'\n", buffSize, buffer);
    int assigned = 0, consumed = 0, consumedStep = 0;
    if (model->size > 0 && model->size % params->k == 0) {
        model->clusters = realloc(model->clusters, model->size + params->k * sizeof(Cluster));
    }
    //
    Cluster *currCluster = &(model->clusters[model->size]);
    char *labelString;
    assigned += sscanf(buffer, "%10u, %m[a-zA-z0-9], %10u, %le, %le, %le%n",
                       &currCluster->id, &labelString, &currCluster->n_matches,
                       &currCluster->distanceAvg, &currCluster->distanceStdDev,
                       &currCluster->radius, &consumedStep
    );
    consumed += consumedStep;
    if (consumed >= buffSize) return 0;
    // "         1, N,        311, 9.625003e-02, 1.105641e+00, 1.105641e-01" // 67
    // fprintf(stderr, "id=%10u, l=%s, n=%10u, dAvg=%le, dDev=%le, r=%le, assingned=%d, consumed=%d\n",
    //         currCluster->id, labelString, currCluster->n_matches,
    //         currCluster->distanceAvg, currCluster->distanceStdDev, currCluster->radius,
    //         assigned, consumed);
    // assertEquals(assigned, 7);
    if (assigned != 6)
        // errx(EXIT_FAILURE, "Assert error, expected '%d' and got '%d'. At "__FILE__":%d\n", 6, assigned, __LINE__);
        return 0;
    if (labelString[1] == '\0' && isalpha(labelString[0])) {
        // single char
        currCluster->label = labelString[0];
    } else {
        currCluster->label = atoi(labelString);
    }
    free(labelString);
    //
    // ", 1.574879e-03, 2.128046e-02, 4.975385e-02, 5.661461e-02, 1.000000e+00, 5.723473e-03, 1.565916e-02, 3.270096e-02, 4.614469e-01, 0.000000e+00, 2.893891e-04, 4.469453e-03, 0.000000e+00, 0.000000e+00, 1.000000e+00, 0.000000e+00, 0.000000e+00, 0.000000e+00, 0.000000e+00, 1.000000e+00, 0.000000e+00, 0.000000e+00" 308
    // ", 1.574879e-03" // 14
    if (currCluster->center == NULL) {
        currCluster->center = calloc(params->dim, sizeof(double));
    }
    for (unsigned int d = 0; d < params->dim; d++) {
        if (consumed >= buffSize) return 0;
        assigned += sscanf(&buffer[consumed], ", %le%n", &currCluster->center[d], &consumedStep);
        consumed += consumedStep;
        // fprintf(stderr, "d=%d, v=%le, assingned=%d, consumedStep=%d, consumed=%d\n", d, currCluster->center[d], assigned, consumedStep, consumed);
    }
    // assertEquals(consumed, buffSize);
    if (consumed != buffSize)
        errx(EXIT_FAILURE, "Assert error, expected 'buffSize' '%lu' and got 'consumed' '%d'. At "__FILE__":%d\n",
             buffSize, consumed, __LINE__);
    // assigned += sscanf(buffer, "\0%n", &consumedStep);
    // consumed += consumedStep;
    // fprintf(stderr, "Cluster(id=%u, l=%s, n=%u, dAvg=%le, dDev=%le, r=%le, assingned=%d)\n",
    //         currCluster->id, printableLabel(currCluster->label), currCluster->n_matches,
    //         currCluster->distanceAvg, currCluster->distanceStdDev, currCluster->radius,
    //         assigned);
    // fprintf(stderr, "Cluster(id=%u)\n", currCluster->id);
    model->size++;
    return consumed;
}

int modelStoreComm(Params *params, int timeout, Model *model, SOCKET modelStore, struct pollfd *modelStorePoll, char *buffer, size_t maxBuffSize) {
    size_t prevSize = model->size;
    while (poll(modelStorePoll, 1, timeout) != 0 && modelStorePoll->revents != 0 && modelStorePoll->revents & POLLIN) {
        bzero(buffer, maxBuffSize);
        ssize_t buffRead = read(modelStore, buffer, maxBuffSize - 1);
        if (buffRead < 0)
            errx(EXIT_FAILURE, "ERROR reading from socket. At "__FILE__":%d\n", __LINE__);
        if (buffRead == 0)
            fprintf(stderr, "EOF model\n");
        // fprintf(stderr, "buffer \t'%s'\n", buffer);
        int consumed = 0, lineSize = 0;
        char *line;
        while (consumed != buffRead) {
            line = &buffer[lineSize];
            while (buffer[lineSize] != '\n' && lineSize < buffRead) lineSize++;
            // printf("line (%d) \t'%s' /%s\n", lineSize, line, &line[lineSize - 1]);
            if (lineSize == 0 || line[0] == '\0' || line[lineSize] == '\0') {
                break;
            }
            if (buffer[lineSize] == '\n') {
                buffer[lineSize] = '\0';
            } else {
                fprintf(stderr, "incomplete line '%s'\n", line);
                // if (lineSize >= buffRead) {
                // compact buffer and read;
                fprintf(stderr, "compact buffer lineSize=%d, buffRead=%ld\n", lineSize, buffRead);
                for (size_t i = 0; i < lineSize && (lineSize + i) < buffRead; i++) {
                    buffer[i] = buffer[i + lineSize];
                }
                int locBuffRead = read(modelStore, &buffer[lineSize - 1], maxBuffSize - lineSize - 1);
                fprintf(stderr, "compact buffer locBuffRead=%d\n", locBuffRead);
                if (locBuffRead < 0)
                    errx(EXIT_FAILURE, "ERROR reading from socket. At "__FILE__":%d\n", __LINE__);
                if (locBuffRead == 0) {
                    buffRead = 0;
                    break;
                }
                buffRead = locBuffRead + lineSize;
                lineSize = 0;
                continue;
            }
            if (line[0] == '#' || lineSize == 0) {
                lineSize++;
                continue;
            }
            consumed += appendClusterFromStore(params, modelStore, line, lineSize, model);
        }
        // fprintf(stderr, "buffRead %d\n", buffRead);
    }
    if (prevSize < model->size)
        fprintf(stderr, "Model(size=%d)\n", model->size);
    return model->size;
}

int classifier(Params *params, Model *model, SOCKET modelStore, struct pollfd *modelStorePoll, SOCKET noveltyDetectionService, char *buffer, size_t maxBuffSize) {
    clock_t start = clock();
    unsigned int id = 0;
    Match match;
    Example example;
    example.val = calloc(params->dim, sizeof(double));
    printf("#pointId,label\n");
    int hasEmptyline = 0;
    unsigned int unknowns = 0;
    while (!feof(stdin) && hasEmptyline != 2) {
        for (size_t d = 0; d < params->dim; d++) {
            assertEquals(scanf("%lf,", &example.val[d]), 1);
        }
        // ignore class
        char class;
        assertEquals(scanf("%c", &class), 1);
        example.id = id;
        id++;
        scanf("\n%n", &hasEmptyline);
        //
        identify(params, model, &example, &match);
        printf("%10u,%s\n", example.id, printableLabel(match.label));
        //
        if (match.label != UNK_LABEL) continue;
        // send to novelty detection service
        unknowns++;
        bzero(buffer, maxBuffSize);
        int offset = sprintf(buffer, "%10u", example.id);
        for (size_t d = 0; d < params->dim; d++) {
            offset += sprintf(&buffer[offset], ", %le", example.val[d]);
        }
        offset += sprintf(&buffer[offset], "\n");
        write(noveltyDetectionService, buffer, offset);
        //
        modelStoreComm(params, 0, model, modelStore, modelStorePoll, buffer, maxBuffSize);
    }
    fprintf(stderr, "unknowns = %u\n", unknowns);
    printTiming(id);
    return id;
}

int main(int argc, char const *argv[]) {
    if (argc == 2) {
        fprintf(stderr, "reading from file %s\n", argv[1]);
        stdin = fopen(argv[1], "r");
    }
    Params *params = calloc(1, sizeof(Params));
    params->executable = argv[0];
    fprintf(stderr, "%s\n", params->executable);
    getParams((*params));

    // Model *model = training(&params);

    Model *model = calloc(1, sizeof(Model));
    model->size = 0;
    model->clusters = calloc(params->k, sizeof(Cluster));
    // 
    int maxBuffSize = 1024;
    char *buffer = calloc(maxBuffSize, sizeof(char));
    //
    SOCKET modelStore = clientConnect("localhost", MODEL_STORE_PORT);
    struct pollfd modelStorePoll;
    modelStorePoll.fd = modelStore;
    modelStorePoll.events = POLLIN;
    // int ready = poll(&pfDs, 1, 2);
    // if (ready == 0 ||pfDs.revents == 0) {
    //     fprintf(stderr, "No events in poll\n");
    //     break;
    // }
    // if (pfDs.revents & POLLIN)
    modelStoreComm(params, 2, model, modelStore, &modelStorePoll, buffer, maxBuffSize);

    SOCKET noveltyDetectionService = clientConnect("localhost", 7001);
    // minasOnline(&params, model);
    classifier(params, model, modelStore, &modelStorePoll, noveltyDetectionService, buffer, maxBuffSize);

    return EXIT_SUCCESS;
}
