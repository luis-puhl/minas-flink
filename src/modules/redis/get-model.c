#include <stdio.h>
#include <err.h>
#include <stdlib.h>

#include <hiredis/hiredis.h>
// #include <hiredis/adapters/libev.h>
// #include <hiredis/hiredis_ssl.h>

const char *const redis_reply_strings[] = {"REDIS_REPLY_STRING", "REDIS_REPLY_ARRAY", "REDIS_REPLY_INTEGER", "REDIS_REPLY_NIL", "REDIS_REPLY_STATUS", "REDIS_REPLY_ERROR", "REDIS_REPLY_DOUBLE", "REDIS_REPLY_BOOL", "REDIS_REPLY_MAP", "REDIS_REPLY_SET", "REDIS_REPLY_ATTR", "REDIS_REPLY_PUSH", "REDIS_REPLY_BIGNUM", "REDIS_REPLY_VERB"};

void mfogPushHandler(void *privdata, void *reply) {
    /* Handle the reply */
    printf("mfogPushHandler: %s", reply == NULL ? NULL : ((redisReply*) reply)->str);

    /* Note: We need to free the reply in our custom handler for
            blocking contexts.  This lets us keep the reply if
            we want. */
    freeReplyObject(reply);
}

int printReply(const char* request, redisReply* reply) {
    if (reply == NULL) {
        return printf("%s r: (null)\n", request);
    }
    return printf("%s r: %d (%s) %s\n", request, reply->type, redis_reply_strings[reply->type - 1], reply->str);
}

#define getPrintFree() \
    redisGetReply(c, (void **)&reply);\
    if (reply == NULL || c->err != 0) { \
        errx(EXIT_FAILURE, "Redis error %d '%s': At "__FILE__":%d\n", c->err, c->errstr, __LINE__);\
    } \
    printf("%d %s %s\n", reply->type, reply->vtype, reply->str); \
    freeReplyObject(reply);

int main(int argc, char const *argv[]) {
    const char* remoteAddr = 
        "127.0.0.1";
        // "ec2-18-191-2-174.us-east-2.compute.amazonaws.com";
    redisContext *c = redisConnect(remoteAddr, 6379);
    // redisContext *c = redisConnectNonBlock(remoteAddr, 6379);
    if (c == NULL || c->err != 0) {
        errx(EXIT_FAILURE, "Redis error %d '%s': At "__FILE__":%d\n", c->err, c->errstr, __LINE__);
    }
    redisSetPushCallback(c, NULL);
    // redisSetPushCallback(c, mfogPushHandler);
    // 
    // printf("Setting to-block\t");
    // c->flags = c->flags ^ REDIS_BLOCK;
    // printf("%x (%x)\n", c->flags, c->flags & REDIS_BLOCK);
    // 
    redisReply *reply;
    /* Switch to the RESP3 protocol */
    // reply = redisCommand(c, "HELLO 3");
    // if (reply == NULL || c->err || reply->type != REDIS_REPLY_MAP) {
    //     errx(EXIT_FAILURE, "Redis error %d '%s': At "__FILE__":%d\n", c->err, c->errstr, __LINE__);
    // }
    // printReply("HELLO 3", reply);
    // freeReplyObject(reply);
    redisAppendCommand(c, "HELLO 3");
    redisAppendCommand(c, "CLIENT TRACKING ON");
    redisAppendCommand(c, "PING");
    redisAppendCommand(c, "LRANGE model-complete 0 -1");
    //
    
    // getPrintFree(); // pong
    while (redisGetReply(c, (void **)&reply) == REDIS_OK && reply == NULL);
    // redisGetReply(c, (void **)&reply);
    if (reply != NULL) {
        printReply("TRACKING", reply);
        freeReplyObject(reply);
        reply = NULL;
    }
    // getPrintFree(); // RESP3
    redisGetReply(c, (void **)&reply);
    if (reply != NULL) {
        printReply("PING", reply);
        freeReplyObject(reply);
    }
    redisGetReply(c, (void **)&reply);
    if (reply == NULL || c->err != 0) {
        errx(EXIT_FAILURE, "Redis error %d '%s': At "__FILE__":%d\n", c->err, c->errstr, __LINE__);
    }
    if (reply != NULL) {
        printReply("LRANGE model-complete", reply);
        // initial model
        if (reply->type == REDIS_REPLY_ARRAY) {
            for (size_t j = 0; j < reply->elements; j++) {
                printf("\t%lu) %s\n", j, reply->element[j]->str);
            }
        }
        freeReplyObject(reply);
    }
    // 
    printf("SUBSCRIBE model\n");
    reply = redisCommand(c, "SUBSCRIBE model");
    if (c->err) {
        errx(EXIT_FAILURE, "Redis error %d '%s': At "__FILE__":%d\n", c->err, c->errstr, __LINE__);\
    }
    printReply("SUBSCRIBE model", reply);
    // reply = NULL;
    printf("Setting non-block\t");
    c->flags = c->flags ^ REDIS_BLOCK;
    printf("%x (%x)\n", c->flags, c->flags & REDIS_BLOCK);
    //
    // while (redisGetReply(c, (void**) &reply) == REDIS_OK && reply == NULL && !c->err) {
    //     // NOPE;
    // }
    // printf("SUBSCRIBE model r: %d %s %s\n", reply->type, reply->vtype, reply->str);
    // freeReplyObject(reply);
    // process stdin stream
    while (!feof(stdin)) {
        // printf("in?\t"); scanf("%*s");
        // model updates
        // c->
        int isOk = redisGetReply(c, (void**) &reply);
        if (isOk == REDIS_ERR || c->err != 0) {
            errx(EXIT_FAILURE, "Redis error %d '%s': At "__FILE__":%d\n", c->err, c->errstr, __LINE__);
        }
        if (isOk == REDIS_OK && reply != NULL) {
            printf("Non block reply: %d %s %s\n", reply->type, reply->vtype, reply->str);
            freeReplyObject(reply);
        }
    }
    // close connetion and context
    redisFree(c);
    return 0;
}
