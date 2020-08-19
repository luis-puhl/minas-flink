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
    modelStoreComm(params, 3, model, modelStore, &modelStorePoll, buffer, maxBuffSize);

    SOCKET noveltyDetectionService = clientConnect("localhost", 7001);
    // minasOnline(&params, model);
    classifier(params, model, modelStore, &modelStorePoll, noveltyDetectionService, buffer, maxBuffSize);

    return EXIT_SUCCESS;
}
