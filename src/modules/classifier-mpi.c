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

#include <hiredis/hiredis.h>

#define MAIN

#include "../baseline/base.h"
#include "../baseline/kmeans.h"
#include "../baseline/clustream.h"
#include "../baseline/minas.h"
#include "../mpi/mfog-mpi.h"

#include "./modules.h"
#include "./redis/redis-connect.h"

#define fail(c) \
    if (c->err != 0) errx(EXIT_FAILURE, "Redis error %d '%s': At "__FILE__":%d\n", c->err, c->errstr, __LINE__);

int classifierRedis(Params *params, Model *model, char *buffer, size_t maxBuffSize) {
    clock_t start = clock();
    //
    redisContext *redisCtx = makeConnection(params, model);
    //
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
        // modelStoreComm
        redisReply *reply;
        void **replyPtr = (void **)&reply;
        int gotPush = 0;
        while (1) {
            if (redisGetReplyFromReader(redisCtx, replyPtr) == REDIS_OK && reply != NULL) {
                fprintf(stderr, "%s\n", get_redis_reply_strings(reply->type));
                gotPush++;
                freeReplyObject(reply);
            } else {
                break;
            }
            fail(redisCtx);
        }
        if (gotPush) {
            // printf("got %d pushes, ask for range (%d, -1)\n", gotPush, modelSize);
            reply = redisCommand(redisCtx, "LRANGE " MODEL_STORE_MODEL_LIST " %d -1", model->size);
            fail(redisCtx);
            // printReply("LRANGE update", reply);
            if (reply->type != REDIS_REPLY_ARRAY) {
                errx(EXIT_FAILURE, "Redis error, Expected ARRAY and didn't get it: At "__FILE__":%d\n", __LINE__);
            }
            for (size_t j = 0; j < reply->elements; j++) {
                appendClusterFromStore(params, reply->element[j]->str, reply->element[j]->len, model);
            }
            freeReplyObject(reply);
            printf("model size = %d\n", model->size);
        }
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
        //
        reply = redisCommand(redisCtx, "RPUSH " MODEL_STORE_UNKNOWNS_LIST " %s", buffer);
        if (reply == NULL) {
            errx(EXIT_FAILURE, "Redis error: %d. At "__FILE__":%d\n", redisCtx->err, __LINE__);
        }
        freeReplyObject(reply);
        //
        reply = redisCommand(redisCtx, "PUBLISH " MODEL_STORE_UNKNOWNS_CH " %s", buffer);
        if (reply == NULL) {
            errx(EXIT_FAILURE, "Redis error: %d. At "__FILE__":%d\n", redisCtx->err, __LINE__);
        }
        freeReplyObject(reply);
    }
    fprintf(stderr, "unknowns = %u\n", unknowns);
    printTiming(id);
    return id;
}

int main(int argc, char const *argv[], char *env[]) {
    if (argc == 2) {
        fprintf(stderr, "reading from file %s\n", argv[1]);
        stdin = fopen(argv[1], "r");
    }
    Params *params = calloc(1, sizeof(Params));
    params->executable = argv[0];
    fprintf(stderr, "%s\n", params->executable);
    getParams((*params));
    // scanf("remoteRedis" "=" "%s" "\n", params->remoteRedis);
    // params->remoteRedis = "ec2-18-191-2-174.us-east-2.compute.amazonaws.com";
    params->remoteRedis = "localhost";
    fprintf(stderr, "\t" "remoteRedis" " = " "%s" "\n", params->remoteRedis);

    Model *model = calloc(1, sizeof(Model));
    model->size = 0;
    model->clusters = calloc(params->k, sizeof(Cluster));
    //
    int maxBuffSize = 1024;
    char *buffer = calloc(maxBuffSize, sizeof(char));
    //
    classifierRedis(params, model, buffer, maxBuffSize);
    return EXIT_SUCCESS;
}
