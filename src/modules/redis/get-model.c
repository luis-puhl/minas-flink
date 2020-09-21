#include <stdio.h>

#include <hiredis/hiredis.h>
// #include <hiredis/hiredis_ssl.h>

int main(int argc, char const *argv[]) {
    // redisContext *c = redisConnect("127.0.0.1", 6379);
    redisContext *c = redisConnect("ec2-18-191-2-174.us-east-2.compute.amazonaws.com", 6379);
    if (c == NULL || c->err) {
        if (c) {
            printf("Error: %s\n", c->errstr);
            // handle error
        } else {
            printf("Can't allocate redis context\n");
        }
    }
    redisReply *reply;
    reply = redisCommand(c, "PING");
    if (reply != NULL) {
        printf("PING: %s\n", reply->str);
        freeReplyObject(reply);
    }
    reply = redisCommand(c,"LRANGE model 0 -1");
    if (reply != NULL) {
        if (reply->type == REDIS_REPLY_ARRAY) {
            for (size_t j = 0; j < reply->elements; j++) {
                printf("%lu) %s\n", j, reply->element[j]->str);
            }
        }
        freeReplyObject(reply);
    }
    redisFree(c);
    return 0;
}
