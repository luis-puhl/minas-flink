#ifndef _MFOG_MPI_H
#define _MFOG_MPI_H 1

#include <mpi.h>

#include "../base/base.h"
#include "../base/minas.h"

#define MPI_RETURN if (mpiReturn != MPI_SUCCESS) { MPI_Abort(MPI_COMM_WORLD, mpiReturn); errx(EXIT_FAILURE, "MPI Abort %d\n", mpiReturn); }
#define MFOG_MAIN_RANK 0

int tradeModel(Params *params, Model *model);
Example* tradeExample(Params *params, Example *example, char *exampleBuffer, int exampleBufferSize, int *dest, double *valuePtr);
Match* tradeMatch(Params *params, Match *match, char *matchBuffer, int matchBufferSize, int *dest, double *valuePtr);

#endif // _MFOG_MPI_H
