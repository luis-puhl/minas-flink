#ifndef _MFOG_REDIS_CONNECT_C
#define _MFOG_REDIS_CONNECT_C

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

#include "../../baseline/base.h"
#include "../../baseline/kmeans.h"
#include "../../baseline/clustream.h"
#include "../../baseline/minas.h"

#include "../../util/net.h"

#include "../modules.h"

#include "./redis-connect.h"

const char *const redis_reply_strings[] = {
    "(nil)", "REDIS_REPLY_STRING", "REDIS_REPLY_ARRAY", "REDIS_REPLY_INTEGER",
    "REDIS_REPLY_NIL", "REDIS_REPLY_STATUS", "REDIS_REPLY_ERROR",
    "REDIS_REPLY_DOUBLE", "REDIS_REPLY_BOOL", "REDIS_REPLY_MAP",
    "REDIS_REPLY_SET", "REDIS_REPLY_ATTR", "REDIS_REPLY_PUSH",
    "REDIS_REPLY_BIGNUM", "REDIS_REPLY_VERB"};

const char* get_redis_reply_strings(int code) {
    return redis_reply_strings[code];
}

int printReply(const char* request, redisReply* reply) {
    if (reply == NULL) {
        return printf("%6.6s r: (null)\n", request);
    }
    const char* repTypeString = redis_reply_strings[reply->type];
    switch (reply->type){
    case REDIS_REPLY_PUSH:
        return printf("%6.6s r: %2.2d-%s\n", request, reply->type, repTypeString);
    case REDIS_REPLY_INTEGER:
        return printf("%6.6s r: %2.2d-%s %lld\n", request, reply->type, repTypeString, reply->integer);
    case REDIS_REPLY_ARRAY:
        return printf("%6.6s r: %2.2d-%s\n"
            "\t[0]: %s\n\t[%ld]: %s\n", request, reply->type, repTypeString,
            reply->element[0]->str, reply->elements, reply->element[reply->elements - 1]->str);
    default:
        return printf("%6.6s r: %2.2d-%s %s\n", request, reply->type, repTypeString, reply->str);
    }
}

redisContext* makeConnection(Params *params, Model *model) {
    clock_t start = clock();
    //
    redisContext *redisCtx = redisConnect(params->remoteRedis, MODEL_STORE_REMOTE_REDIS_PORT);
    if (redisCtx == NULL) {
        errx(EXIT_FAILURE, "Redis error %d '%s': At "__FILE__":%d\n", 0, "", __LINE__);
    }
    redisSetPushCallback(redisCtx, NULL);
    fail(redisCtx);
    redisReply *reply;
    void **replyPtr = (void **) &reply;
    //
    redisAppendCommand(redisCtx, "HELLO 3");
    redisAppendCommand(redisCtx, "CLIENT TRACKING ON");
    redisAppendCommand(redisCtx, "LRANGE " MODEL_STORE_MODEL_LIST " 0 -1");
    // hello
    redisGetReply(redisCtx, replyPtr);
    fail(redisCtx);
    freeReplyObject(reply);
    // tracking
    redisGetReply(redisCtx, replyPtr);
    fail(redisCtx);
    freeReplyObject(reply);
    // model
    redisGetReply(redisCtx, replyPtr);
    fail(redisCtx);
    if (reply->type != REDIS_REPLY_ARRAY) {
        errx(EXIT_FAILURE, "Redis error, Expected ARRAY and didn't get it: At "__FILE__":%d\n", __LINE__);
    }
    int nClusters = reply->elements;
    for (size_t j = 0; j < nClusters; j++) {
        appendClusterFromStore(params, reply->element[j]->str, reply->element[j]->len, model);
    }
    freeReplyObject(reply);
    //
    // int gotPush = 0;
    // while (1) {
    //     if (redisGetReplyFromReader(redisCtx, replyPtr) == REDIS_OK && reply != NULL) {
    //         fprintf(stderr, "%s\n", get_redis_reply_strings(reply->type));
    //         gotPush++;
    //         freeReplyObject(reply);
    //     } else {
    //         break;
    //     }
    //     fail(redisCtx);
    // }
    // freeReplyObject(reply);
    //
    printTiming(nClusters);
    return redisCtx;
}

#endif // !_MFOG_REDIS_CONNECT_C
