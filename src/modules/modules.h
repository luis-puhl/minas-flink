#ifndef _MODULES_H
#define _MODULES_H 1

// #include <unistd.h>
// #include <sys/types.h>
// #include <sys/socket.h>
// #include <sys/select.h>
// #include <netinet/in.h>
// #include <netdb.h>
// #include <arpa/inet.h>
#include <poll.h>

#include "../baseline/base.h"
#include "../util/net.h"

#define MODEL_STORE_PORT 7000
#define MODEL_STORE_REMOTE_REDIS_PORT 6379

#define MODEL_STORE_UNKNOWNS_LIST "minas-unkowns"
#define MODEL_STORE_UNKNOWNS_CH "minas-unkowns-ch"

#define MODEL_STORE_MODEL_LIST "minas-clusters"
#define MODEL_STORE_MODEL_UPDATE_CHANNEL "model-up-ch"
// #define MODEL_STORE_REQUEST_GET_ALL "get complete model\n"
// #define MODEL_STORE_REQUEST_SEND_ALL "# Model(dimension="

int appendClusterFromStore(Params *params, char *buffer, size_t buffSize, Model *model);
int modelStoreComm(Params *params, int timeout, Model *model, SOCKET modelStore, struct pollfd *modelStorePoll, char *buffer, size_t maxBuffSize);

#endif // _MODULES_H
