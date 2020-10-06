#ifndef _MFOG_MPI_C
#define _MFOG_MPI_C 1

#include "./mfog-mpi.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <err.h>
#include <string.h>
#include <mpi.h>

#include "../base/base.h"
#include "../base/minas.h"

#include "./mfog-mpi.h"

// #define MPI_RETURN if (mpiReturn != MPI_SUCCESS) { MPI_Abort(MPI_COMM_WORLD, mpiReturn); errx(EXIT_FAILURE, "MPI Abort %d\n", mpiReturn); }
// #define MFOG_MASTER_RANK 0

int tradeModel(Params *params, Model *model) {
    int mpiReturn, bufferSize;
    // block everyone
    MPI_Barrier(MPI_COMM_WORLD);
    if (params->mpiRank == 0) {
        bufferSize = sizeof(Model) + (model->size) * sizeof(Cluster) + params->dim * (model->size) * sizeof(double);
        mpiReturn = MPI_Bcast(&bufferSize, 1, MPI_INT, MFOG_MAIN_RANK, MPI_COMM_WORLD);
        MPI_RETURN
    } else {
        mpiReturn = MPI_Bcast(&bufferSize, 1, MPI_INT, MFOG_MAIN_RANK, MPI_COMM_WORLD);
        MPI_RETURN
    }
    char *buffer = malloc(bufferSize);
    int position = 0;
    if (params->mpiRank == 0) {
        mpiReturn = MPI_Pack(model, sizeof(Model), MPI_BYTE, buffer, bufferSize, &position, MPI_COMM_WORLD);
        MPI_RETURN
        mpiReturn = MPI_Pack(model->clusters, model->size * sizeof(Cluster), MPI_BYTE, buffer, bufferSize, &position, MPI_COMM_WORLD);
        MPI_RETURN
        for (int i = 0; i < model->size; i++) {
            mpiReturn = MPI_Pack(model->clusters[i].center, params->dim, MPI_DOUBLE, buffer, bufferSize, &position, MPI_COMM_WORLD);
            MPI_RETURN
        }
        if (position != bufferSize) errx(EXIT_FAILURE, "Buffer sizing error. Used %d of %d.\n", position, bufferSize);
        mpiReturn = MPI_Bcast(buffer, position, MPI_PACKED, MFOG_MAIN_RANK, MPI_COMM_WORLD);
        MPI_RETURN
    } else {
        mpiReturn = MPI_Bcast(buffer, bufferSize, MPI_PACKED, MFOG_MAIN_RANK, MPI_COMM_WORLD);
        MPI_RETURN
        model = malloc(sizeof(Model));
        mpiReturn = MPI_Unpack(buffer, bufferSize, &position, model, sizeof(Model), MPI_BYTE, MPI_COMM_WORLD);
        MPI_RETURN
        model->clusters = malloc(model->size * sizeof(Cluster));
        mpiReturn = MPI_Unpack(buffer, bufferSize, &position, model->clusters, model->size * sizeof(Cluster), MPI_BYTE, MPI_COMM_WORLD);
        MPI_RETURN
        for (int i = 0; i < model->size; i++) {
            model->clusters[i].center = malloc(params->dim * sizeof(double));
            mpiReturn = MPI_Unpack(buffer, bufferSize, &position, model->clusters[i].center, params->dim, MPI_DOUBLE, MPI_COMM_WORLD);
            MPI_RETURN
        }
        // fprintf(stderr, "[%d] Recv model with %d clusters took \t%es\n", clRank, model->size, ((double)(clock() - start)) / ((double)1000000));
    }
    free(buffer);
    return mpiReturn;
}

Example* tradeExample(Params *params, Example *example, char *exampleBuffer, int exampleBufferSize, int *dest, double *valuePtr) {
    if (params->mpiRank == MFOG_MAIN_RANK) {
        int position = 0;
        MPI_Pack(example, sizeof(Example), MPI_BYTE, exampleBuffer, exampleBufferSize, &position, MPI_COMM_WORLD);
        MPI_Pack(example->val, params->dim, MPI_DOUBLE, exampleBuffer, exampleBufferSize, &position, MPI_COMM_WORLD);
        MPI_Send(exampleBuffer, position, MPI_PACKED, *dest, 2004, MPI_COMM_WORLD);
        *dest = ++*dest < params->mpiSize ? *dest : 1;
    } else {
        MPI_Recv(exampleBuffer, exampleBufferSize, MPI_PACKED, MPI_ANY_SOURCE, 2004, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        int position = 0;
        MPI_Unpack(exampleBuffer, exampleBufferSize, &position, example, sizeof(Example), MPI_BYTE, MPI_COMM_WORLD);
        if (example->id < 0) return 0;
        MPI_Unpack(exampleBuffer, exampleBufferSize, &position, valuePtr, params->dim, MPI_DOUBLE, MPI_COMM_WORLD);
        example->val = valuePtr;
    }
    return example;
}

Match* tradeMatch(Params *params, Match *match, char *matchBuffer, int matchBufferSize, int *dest, double *valuePtr) {
    if (params->mpiRank == 0) {
        int hasMessage = 0;
        MPI_Iprobe(MPI_ANY_SOURCE, 2005, MPI_COMM_WORLD, &hasMessage, MPI_STATUS_IGNORE);
        if (hasMessage) {
            MPI_Recv(match, sizeof(Match), MPI_BYTE, MPI_ANY_SOURCE, 2005, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
    } else {
        MPI_Send(match, sizeof(Match), MPI_BYTE, MFOG_MAIN_RANK, 2005, MPI_COMM_WORLD);
    }
    return match;
}

#endif // _MFOG_MPI_C
