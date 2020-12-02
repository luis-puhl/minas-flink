#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <hiredis/hiredis.h>
#include <mpi.h>

#include "../../base/base.h"
#include "../../base/minas.h"

#include "../modules.h"
#include "./redis-connect.h"

int modelStoreUpdateRedis(Params *params, redisContext *redisCtx, Model *model);
int sendUnknownRedis(Params *params, redisContext *redisCtx, Example *example, char *buffer, size_t maxBuffSize);
