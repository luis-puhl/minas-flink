#include <stdio.h>
#include <stdlib.h>
#include <err.h>
// #include <string.h>
// #include <math.h>
#include <time.h>
// #include <ctype.h>

#include <hiredis/hiredis.h>

#define MAIN

#include "../baseline/base.h"
// #include "../baseline/kmeans.h"
// #include "../baseline/clustream.h"
// #include "../baseline/minas.h"

#include "../util/net.h"

#include "./modules.h"
#include "./redis/redis-connect.h"

int main(int argc, char const *argv[], char *env[]) {
    clock_t start = clock();
    if (argc == 2) {
        fprintf(stderr, "reading from file %s\n", argv[1]);
        stdin = fopen(argv[1], "r");
    }
    Params *params = calloc(1, sizeof(Params));
    params->executable = argv[0];
    fprintf(stderr, "%s\n", params->executable);
    getParams((*params));
    params->remoteRedis = "localhost"; // "ec2-18-191-2-174.us-east-2.compute.amazonaws.com";
    fprintf(stderr, "\t" "remoteRedis" " = " "%s" "\n", params->remoteRedis);

    Model *model = calloc(1, sizeof(Model));
    model->size = 0;
    model->clusters = calloc(params->k, sizeof(Cluster));
    //
    // int maxBuffSize = 1024;
    // char *buffer = calloc(maxBuffSize, sizeof(char));
    //
    // redisContext *redisCtx = makeConnection(params, model);
    redisContext *redisCtx = redisConnect(params->remoteRedis, MODEL_STORE_REMOTE_REDIS_PORT);
    //
    redisReply *reply;
    void **replyPtr = (void **)&reply;
    reply = redisCommand(redisCtx, "LLEN " MODEL_STORE_UNKNOWNS_LIST);
    printReply("LLEN", reply);
    //
    int gotPush = 0;
    int counter = 10;
    reply = redisCommand(redisCtx, "SUBSCRIBE " MODEL_STORE_UNKNOWNS_CH);
    printReply("SUBSCRIBE", reply);
    fflush(stdout);
    while (counter) {
        if (redisGetReply(redisCtx, replyPtr) == REDIS_OK && reply != NULL) {
            fprintf(stderr, "%s\n", get_redis_reply_strings(reply->type));
            printReply("Loop", reply);
            gotPush++;
            freeReplyObject(reply);
        }
        if (redisGetReplyFromReader(redisCtx, replyPtr) == REDIS_OK && reply != NULL) {
            printReply("Loop", reply);
            printf("r: %d (%s) %s\n", reply->type, get_redis_reply_strings(reply->type), reply->str);
            gotPush++;
            freeReplyObject(reply);
        } else {
            // break;
            counter--;
        }
        fail(redisCtx);
    }
    reply = redisCommand(redisCtx, "LLEN " MODEL_STORE_UNKNOWNS_LIST);
    printReply("LLEN", reply);
    printf("final unknowns = %lld\n", reply->integer);
    printf("final counter = %d\n", counter);
    printf("final gotPush = %d\n", gotPush);
    fflush(stdout);
    //
    printTiming(model->size);
    return EXIT_SUCCESS;
}
