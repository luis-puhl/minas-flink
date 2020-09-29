#ifndef _ND_SERVICE_H
#define _ND_SERVICE_H 1

#include "./minas.h"
#include "../mpi/minas-mpi.h"
#include "../util/loadenv.h"
#include "../util/net.h"

#define MODEL_SERVER_PORT 7200

void handleUnknown(mfog_params_t *params, Model *model);
void noveltyDetectionService(SOCKET connection, mfog_params_t *params, char *buffer, size_t maxBuff, Model *model);
Model *modelStoreService(mfog_params_t *params);
Model *getModelFromStore(mfog_params_t *params);

void sendUnk(mfog_params_t *params, Point *unk);

#endif // !_ND_SERVICE_H
