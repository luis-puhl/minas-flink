#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <err.h>
#include <string.h>
#include <mpi.h>

typedef struct aPack {
    int aInt;
    char aChar;
    double *doubles;
} APack;

typedef struct bPack {
    APack *aLot;
    int size, dimension;
} BPack;

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);
    int clRank, clSize;
    MPI_Comm_size(MPI_COMM_WORLD, &clSize);
    MPI_Comm_rank(MPI_COMM_WORLD, &clRank);
    BPack b;
    if (clRank == 0) {
        char processor_name[MPI_MAX_PROCESSOR_NAME];
        int name_len;
        MPI_Get_processor_name(processor_name, &name_len);
        fprintf(stderr, "Processor %s, Rank %d out of %d processors\n", processor_name, clRank, clSize);
        //
        b.size = 3;
        b.dimension = 2;
        b.aLot = malloc(b.size * sizeof(APack));
        for (int i = 0; i < b.size; i++) {
            b.aLot[i].aChar = 'a' + (i % 26);
            b.aLot[i].aInt = i;
            b.aLot[i].doubles = malloc(b.dimension * sizeof(double));
            for (int j = 0; j < b.size; j++) {
                b.aLot[i].doubles[i] = ((double) j + i);
            }
        }
        //
        int bufferSize = sizeof(BPack) + b.size * sizeof(APack) + b.size * b.dimension * sizeof(double);
        void *buffer = malloc(bufferSize);
        int position = 0;
        MPI_Pack(&b, sizeof(BPack), MPI_BYTE, buffer, bufferSize, &position, MPI_COMM_WORLD);
        MPI_Pack(b.aLot, b.size * sizeof(APack), MPI_BYTE, buffer, bufferSize, &position, MPI_COMM_WORLD);
        for (int i = 0; i < b.size; i++) {
            MPI_Pack(b.aLot[i].doubles, b.dimension, MPI_DOUBLE, buffer, bufferSize, &position, MPI_COMM_WORLD);
        }
        // #define BUFFER_SIZE_TAG 10
        // MPI_Send(&bufferSize, 1, MPI_INT, 1, BUFFER_SIZE_TAG, MPI_COMM_WORLD);
        #define BCAST_ROOT 0
        MPI_Bcast(&bufferSize, 1, MPI_INT, BCAST_ROOT, MPI_COMM_WORLD);
        MPI_Barrier(MPI_COMM_WORLD);
        MPI_Bcast(buffer, position, MPI_PACKED, 0, MPI_COMM_WORLD);
        free(buffer);
        //
        MPI_Barrier(MPI_COMM_WORLD);
        printf("snd b.size=%d, b.dimension=%d\n", b.size, b.dimension);
        for (int i = 0; i < b.size; i++) {
            printf("\tsnd A char=%c, int=%d, ", b.aLot[i].aChar, b.aLot[i].aInt);
            for (int j = 0; j < b.dimension; j++) {
                printf("x%d=%lf, ", j, b.aLot[i].doubles[j]);
            }
            printf("\n");
        }
        printf("send done\n");
    } else {
        int bufferSize;
        // MPI_Recv(&bufferSize, 1, MPI_INT, MPI_ANY_SOURCE, BUFFER_SIZE_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Bcast(&bufferSize, 1, MPI_INT, BCAST_ROOT, MPI_COMM_WORLD);
        void *buffer = malloc(bufferSize);
        MPI_Barrier(MPI_COMM_WORLD);
        MPI_Bcast(buffer, bufferSize, MPI_PACKED, 0, MPI_COMM_WORLD);
        //
        int position = 0;
        MPI_Unpack(buffer, bufferSize, &position, &b, sizeof(BPack), MPI_BYTE, MPI_COMM_WORLD);
        b.aLot = malloc(b.size * sizeof(APack));
        MPI_Unpack(buffer, bufferSize, &position, b.aLot, b.size * sizeof(APack), MPI_BYTE, MPI_COMM_WORLD);
        MPI_Barrier(MPI_COMM_WORLD);
        printf("rcv b.size=%d, b.dimension=%d\n", b.size, b.dimension);
        for (int i = 0; i < b.size; i++) {
            b.aLot[i].doubles = malloc(b.dimension * sizeof(double));
            MPI_Unpack(buffer, bufferSize, &position, b.aLot[i].doubles, b.dimension, MPI_DOUBLE, MPI_COMM_WORLD);
            printf("\trcv A char=%c, int=%d, ", b.aLot[i].aChar, b.aLot[i].aInt);
            for (int j = 0; j < b.dimension; j++) {
                printf("x%d=%lf, ", j, b.aLot[i].doubles[j]);
            }
            printf("\n");
        }
        free(buffer);
    }
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
    return 0;
}
