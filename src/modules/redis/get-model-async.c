#include <stdio.h>
#include <err.h>
#include <stdlib.h>

#include <hiredis/hiredis.h>

#define fail(c) \
    if (c->err != 0) errx(EXIT_FAILURE, "Redis error %d '%s': At "__FILE__":%d\n", c->err, c->errstr, __LINE__);

const char *const redis_reply_strings[] = {"REDIS_REPLY_STRING", "REDIS_REPLY_ARRAY", "REDIS_REPLY_INTEGER", "REDIS_REPLY_NIL", "REDIS_REPLY_STATUS", "REDIS_REPLY_ERROR", "REDIS_REPLY_DOUBLE", "REDIS_REPLY_BOOL", "REDIS_REPLY_MAP", "REDIS_REPLY_SET", "REDIS_REPLY_ATTR", "REDIS_REPLY_PUSH", "REDIS_REPLY_BIGNUM", "REDIS_REPLY_VERB"};

int printReply(const char* request, redisReply* reply) {
    if (reply == NULL) {
        return printf("%s r: (null)\n", request);
    }
    int counter = printf("%s r: %d (%s) %s\n", request, reply->type, redis_reply_strings[reply->type - 1], reply->str);
    if (reply->type == REDIS_REPLY_ARRAY) {
        for (size_t j = 0; j < reply->elements; j++) {
            counter += printf("\t%lu) %s\n", j, reply->element[j]->str);
        }
    }
    return counter;
}

int main(int argc, char const *argv[]) {
    redisContext *c = redisConnect("127.0.0.1", 6379);
    if (c == NULL) {
        errx(EXIT_FAILURE, "Redis error: At "__FILE__":%d\n", __LINE__);
    }
    fail(c);
    redisSetPushCallback(c, NULL);
    //
    const char *const initCommands[] = {
        "HELLO 3",
        "CLIENT TRACKING ON",
        "PING",
        "LRANGE model-complete 0 -1"
    };
    for (size_t i = 0; i < 4; i++) {
        redisAppendCommand(c, initCommands[i]);
    }
    redisReply *reply;
    fail(c);
    int modelSize = 0;
    for (size_t i = 0; i < 4; i++) {
        if (redisGetReply(c, (void **)&reply) == REDIS_OK && reply != NULL) {
            printReply(initCommands[i], reply);
            if (reply->type == REDIS_REPLY_ARRAY) {
                modelSize += reply->elements;
            }
            freeReplyObject(reply);
        } else {
            printf("fail at get reply\n");
        }
        fail(c);
    }
    printf("model size = %d\n", modelSize);
    //
    reply = redisCommand(c, "SUBSCRIBE model");
    fail(c);
    printReply("SUBSCRIBE model", reply);
    freeReplyObject(reply);
    for (size_t item = 0; item < 10; item++) {
        printf("\t---item %lu---\n", item);
        if (item % 3 == 1) {
            const char* pub = "PUBLISH model %d";
            reply = redisCommand(c, pub, item);
            fail(c);
            printReply(pub, reply);
            freeReplyObject(reply);
        }
        int gotPush = 0;
        while (1) {
            if (redisGetReplyFromReader(c, (void **)&reply) == REDIS_OK && reply != NULL) {
                printReply("SUB PUSH", reply);
                gotPush++;
                freeReplyObject(reply);
            } else {
                break;
            }
            fail(c);
        }
        if (gotPush) {
            printf("got %d pushes, ask for range (%d, -1)\n", gotPush, modelSize);
            reply = redisCommand(c, "LRANGE model-complete %d -1", modelSize);
            fail(c);
            printReply("LRANGE update", reply);
            if (reply->type == REDIS_REPLY_ARRAY) {
                modelSize += reply->elements;
            }
            freeReplyObject(reply);
            printf("model size = %d\n", modelSize);
        }
    }
    printf("\t---DONE with model size = %d---\n", modelSize);
    // close connetion and context
    redisFree(c);
    return 0;
}
