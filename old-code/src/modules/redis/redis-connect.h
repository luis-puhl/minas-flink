#ifndef _MFOG_REDIS_CONNECT_H
#define _MFOG_REDIS_CONNECT_H

#include <hiredis/hiredis.h>

#include "../../base/base.h"
#include "../../base/minas.h"

#include "./redis-commands.h"

#define rdsFail(c) if (c->err != 0) errx(EXIT_FAILURE, "Redis error %d '%s': At "__FILE__":%d\n", c->err, c->errstr, __LINE__);

const char *get_redis_reply_strings(int code);
int printReply(const char *request, redisReply *reply);
redisContext *makeConnection(Params *params, Model *model);

#endif // !_MFOG_REDIS_CONNECT_H
