#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <hiredis/hiredis.h>
#include <mpi.h>

#define MAIN

#include "../../base/base.h"
#include "../../base/minas.h"
// #include "../mpi/mfog-mpi.h"

#include "../modules.h"
#include "./redis-connect.h"

int modelStoreUpdateRedis(Params *params, redisContext *redisCtx, Model *model) {
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
        rdsFail(redisCtx);
    }
    if (gotPush) {
        // printf("got %d pushes, ask for range (%d, -1)\n", gotPush, modelSize);
        reply = redisCommand(redisCtx, "LRANGE " MODEL_STORE_MODEL_LIST " %d -1", model->size);
        rdsFail(redisCtx);
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
    return gotPush;
}

int sendUnknownRedis(Params *params, redisContext *redisCtx, Example *example, char *buffer, size_t maxBuffSize) {
    redisReply *reply;
    bzero(buffer, maxBuffSize);
    int offset = sprintf(buffer, "%10u", example->id);
    for (size_t d = 0; d < params->dim; d++) {
        offset += sprintf(&buffer[offset], ", %le", example->val[d]);
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
    return REDIS_OK;
}
