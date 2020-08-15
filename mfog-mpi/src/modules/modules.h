
#include "../baseline/base.h"
#include "../util/net.h"

#define MODEL_STORE_PORT 7000
// #define MODEL_STORE_REQUEST_GET_ALL "get complete model\n"
// #define MODEL_STORE_REQUEST_SEND_ALL "# Model(dimension="

int appendClusterFromStore(Params *params, SOCKET modelStore, char *buffer, size_t buffSize, Model *model);
int modelStoreComm(Params *params, int timeout, Model *model, SOCKET modelStore, struct pollfd *modelStorePoll, char *buffer, size_t maxBuffSize);