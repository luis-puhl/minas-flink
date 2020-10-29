#include <stdio.h>
#include <stdlib.h>
#include <err.h>
// #include <string.h>
// #include <math.h>
#include <time.h>
// #include <ctype.h>

#include <hiredis/hiredis.h>

#define MAIN

#include "../base/base.h"

#include "./modules.h"
#include "./redis/redis-connect.h"

int main(int argc, char const *argv[], char *env[]) {
    clock_t start = clock();
    Params *params = setup(argc, argv, env);
    Model *model = calloc(1, sizeof(Model));
    size_t unknownsSize;
    model->size = 0;
    model->clusters = calloc(params->k, sizeof(Cluster));
    //
    // int maxBuffSize = 1024;
    // char *buffer = calloc(maxBuffSize, sizeof(char));
    //
    // redisContext *redisCtx = makeConnection(params, model);
    redisContext *redisCtx;
    redisReply *reply;
    void **replyPtr = (void **)&reply;
    if (params->useRedis) {
        redisCtx = redisConnect(params->remoteRedis, MODEL_STORE_REMOTE_REDIS_PORT);
        reply = redisCommand(redisCtx, "LLEN " MODEL_STORE_UNKNOWNS_LIST);
        printReply("LLEN", reply);
        unknownsSize = reply->integer;
        reply = redisCommand(redisCtx, "SUBSCRIBE " MODEL_STORE_UNKNOWNS_CH);
        printReply("SUBSCRIBE", reply);
    } else {
        printf("Reading from stdin\n");
    }
    int gotPush = 0, counter = 10;
    Example *example, *unknowns;
    unknowns = calloc(unknownsSize + 1, sizeof(Example));
    example = calloc(1, sizeof(Example));
    fflush(stdout);
    while (counter) {
        if (params->useRedis) {
            if (redisGetReply(redisCtx, replyPtr) == REDIS_OK && reply != NULL) {
                printReply("Loop gReply", reply);
                gotPush++;
                freeReplyObject(reply);
            }
            if (redisGetReplyFromReader(redisCtx, replyPtr) == REDIS_OK && reply != NULL) {
                printReply("Loop fReader", reply);
                gotPush++;
                freeReplyObject(reply);
            } else {
                // break;
                counter--;
            }
            rdsFail(redisCtx);
        } else {
            if (next(params, &example) == NULL) {
                break;
            }
            unknownsSize++;
            unknowns = realloc(unknowns, unknownsSize * sizeof(Example));
            
        }
    }
    if (params->useRedis) {
        reply = redisCommand(redisCtx, "UNSUBSCRIBE " MODEL_STORE_UNKNOWNS_CH);
        printReply("UNSUBSCRIBE", reply);
        reply = redisCommand(redisCtx, "LLEN " MODEL_STORE_UNKNOWNS_LIST);
        printReply("LLEN", reply);
        unknownsSize = reply->integer;
    } else {
        //
    }
    printf("final unknowns = %ld\n", unknownsSize);
    printf("final counter = %d\n", counter);
    printf("final gotPush = %d\n", gotPush);
    fflush(stdout);
    //
    printTiming(main, model->size);
    printTimeLog(params);
    return EXIT_SUCCESS;
}
