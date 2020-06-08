#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

#include "minas.h"

extern int MNS_dimesion;

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    int clRank, clSize;
    MPI_Comm_size(MPI_COMM_WORLD, &clSize);
    MPI_Comm_rank(MPI_COMM_WORLD, &clRank);
    char processor_name[MPI_MAX_PROCESSOR_NAME];
    int name_len;
    MPI_Get_processor_name(processor_name, &name_len);
    printf("Hello world from processor %s, rank %d out of %d processors\n", processor_name, clRank, clSize);
    
    MNS_dimesion = 22;
    int modelSize;
    Cluster* model;
    double modelStart = MPI_Wtime();
    if (clRank == 0) {
        // init model
        MNS_dimesion = 22;
        modelSize = 100;
        model = malloc(modelSize * sizeof(Cluster));
        for (unsigned int i = 0; i < modelSize; i++) {
            model[i].id = i;
            model[i].center = malloc(MNS_dimesion * sizeof(float));
            for (unsigned int j = 0; j < MNS_dimesion; j++) {
                model[i].center[j] = ((float)j) / 100;
            }
            model[i].label = 'N';
        }
        // send
        for (int destination = 1; destination < clSize; destination++) {
            MPI_Send(&MNS_dimesion, 1, MPI_INT, destination, 2000, MPI_COMM_WORLD);
            MPI_Send(&modelSize, 1, MPI_INT, destination, 2001, MPI_COMM_WORLD);
            for (int i = 0; i < modelSize; i++) {
                MPI_Send(&model[i], sizeof(Cluster), MPI_BYTE, destination, 2002, MPI_COMM_WORLD);
                MPI_Send(model[i].center, MNS_dimesion, MPI_FLOAT, destination, 2003, MPI_COMM_WORLD);
            }
        }
        for (unsigned int i = 0; i < modelSize; i++) {
            free(model[i].center);
        }
        free(model);
    } else {
        MPI_Status status;
        MPI_Recv(&MNS_dimesion, 1, MPI_INT, MPI_ANY_SOURCE, 2000, MPI_COMM_WORLD, &status);
        MPI_Recv(&modelSize, 1, MPI_INT, MPI_ANY_SOURCE, 2001, MPI_COMM_WORLD, &status);
        model = malloc(modelSize * sizeof(Cluster));
        for (int i = 0; i < modelSize; i++) {
            MPI_Recv(&model[i], sizeof(Cluster), MPI_BYTE, MPI_ANY_SOURCE, 2002, MPI_COMM_WORLD, &status);
            float* center = malloc(MNS_dimesion * sizeof(float));
            MPI_Recv(center, MNS_dimesion, MPI_FLOAT, MPI_ANY_SOURCE, 2003, MPI_COMM_WORLD, &status);
            model[i].center = center;
        }
    }
    double modelEnd = MPI_Wtime();
    printf("[%d] Model transaciton in %fs\n", clRank, modelEnd - modelStart);
    
    Point example;
    int remaining;
    if (clRank == 0) {
        example.value = malloc(MNS_dimesion * sizeof(float));
        for (unsigned int j = 0; j < MNS_dimesion; j++) {
            example.value[j] = ((float)j) / 100;
        }
        example.id = 0;
        remaining = 652700;
        while (remaining > 0) {
            for (int destination = 1; destination < clSize; destination++) {
                MPI_Send(&remaining, 1, MPI_INT, destination, 2004, MPI_COMM_WORLD);
                MPI_Send(&example, sizeof(Point), MPI_BYTE, destination, 2005, MPI_COMM_WORLD);
                MPI_Send(example.value, MNS_dimesion, MPI_FLOAT, destination, 2006, MPI_COMM_WORLD);
                example.id++;
                remaining--;
                if (remaining <= 0) break;
            }
        }
        printf("[%d] Done sending, remaining %d\n", clRank, remaining);
        for (int destination = 1; destination < clSize; destination++) {
            MPI_Send(&remaining, 1, MPI_INT, destination, 2004, MPI_COMM_WORLD);
        }
        free(example.value);
    } else {
        MPI_Status status;
        float* value = malloc(MNS_dimesion * sizeof(float));
        remaining = 1;
        while (remaining > 0) {
            MPI_Recv(&remaining, 1, MPI_INT, MPI_ANY_SOURCE, 2004, MPI_COMM_WORLD, &status);
            if (remaining <= 0) break;
            // if (remaining % 1000 == 0) printf("[%d] remaining %d\n", clRank, remaining);
            MPI_Recv(&example, sizeof(Point), MPI_BYTE, MPI_ANY_SOURCE, 2005, MPI_COMM_WORLD, &status);
            MPI_Recv(value, MNS_dimesion, MPI_FLOAT, MPI_ANY_SOURCE, 2006, MPI_COMM_WORLD, &status);
            example.value = value;
            Cluster minCluster;
            float minDistance = -1;
            for (int i = 0; i < modelSize; i++) {
                float distance = 0;
                for (int j = 0; j < MNS_dimesion; j++) {
                    float diff = model[i].center[j] - example.value[j];
                    distance += diff * diff;
                }
                if (minDistance == -1 || minDistance > distance) {
                    minCluster = model[i];
                    minDistance = distance;
                }
            }
            // do nothing with classification result
        }
        free(value);
        for (unsigned int i = 0; i < modelSize; i++) free(model[i].center);
        free(model);
    }
    double classifyEnd = MPI_Wtime();
    printf("[%d] Classification transaciton in %fs\n", clRank, classifyEnd - modelEnd);

    MPI_Finalize();
    exit(EXIT_SUCCESS);
}
