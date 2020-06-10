#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <err.h>
#include <string.h>
#include <mpi.h>

#include "./minas.h"

#define MFOG_MASTER_RANK 0

void sendModel(int dimension, Model *model, int clRank, int clSize) {
    // MPI_Broadcast
    // MPI_Pack
    clock_t start = clock();
    
    int bufferSize = sizeof(Model) +
        (model->size) * sizeof(Cluster) +
        dimension * (model->size) * sizeof(double);
    char *buffer = malloc(bufferSize);
    int position = 0;
    MPI_Pack(model, sizeof(Model), MPI_BYTE, buffer, bufferSize, &position, MPI_COMM_WORLD);
    MPI_Pack(model->vals, model->size * sizeof(Cluster), MPI_BYTE, buffer, bufferSize, &position, MPI_COMM_WORLD);
    for (int i = 0; i < model->size; i++) {
        MPI_Pack(model->vals[i].center, dimension, MPI_DOUBLE, buffer, bufferSize, &position, MPI_COMM_WORLD);
    }
    // position = bufferSize
    if (position != bufferSize) errx(EXIT_FAILURE, "Buffer sizing error. Used %d of %d.\n", position, bufferSize);
    MPI_Bcast(&bufferSize, 1, MPI_INT, MFOG_MASTER_RANK, MPI_COMM_WORLD);
    // MPI_Barrier(MPI_COMM_WORLD);
    MPI_Bcast(buffer, position, MPI_PACKED, MFOG_MASTER_RANK, MPI_COMM_WORLD);
    free(buffer);
    fprintf(stderr, "[%d] Send model with %d clusters took \t%es\n", clRank, model->size, ((double)(clock() - start)) / ((double)1000000));
}

void receiveModel(int dimension, Model *model, int clRank) {
    clock_t start = clock();
    int bufferSize;
    MPI_Bcast(&bufferSize, 1, MPI_INT, MFOG_MASTER_RANK, MPI_COMM_WORLD);
    // fprintf(stderr, "[%d] Model size to recv %d\n", clRank, bufferSize);
    char *buffer = malloc(bufferSize);
    // MPI_Barrier(MPI_COMM_WORLD);
    MPI_Bcast(buffer, bufferSize, MPI_PACKED, MFOG_MASTER_RANK, MPI_COMM_WORLD);

    int position = 0;
    MPI_Unpack(buffer, bufferSize, &position, model, sizeof(Model), MPI_BYTE, MPI_COMM_WORLD);
    model->vals = malloc(model->size * sizeof(Cluster));
    MPI_Unpack(buffer, bufferSize, &position, model->vals, model->size * sizeof(Cluster), MPI_BYTE, MPI_COMM_WORLD);
    for (int i = 0; i < model->size; i++) {
        model->vals[i].center = malloc(model->dimension * sizeof(double));
        MPI_Unpack(buffer, bufferSize, &position, model->vals[i].center, model->dimension, MPI_DOUBLE, MPI_COMM_WORLD);
    }
    free(buffer);
    fprintf(stderr, "[%d] Recv model with %d clusters took \t%es\n", clRank, model->size, ((double)(clock() - start)) / ((double)1000000));
}

int sendExamples(int dimension, Point *examples, int clSize) {
    int dest = 1, hasRemaining = 1, exampleCounter = 0;
    clock_t start = clock();
    for (exampleCounter = 0; examples[exampleCounter].value != NULL; exampleCounter++) {
        Point *ex = &(examples[exampleCounter]);
        MPI_Send(&hasRemaining, 1, MPI_INT, dest, 2004, MPI_COMM_WORLD);
        MPI_Send(ex, sizeof(Point), MPI_BYTE, dest, 2005, MPI_COMM_WORLD);
        MPI_Send(ex->value, dimension, MPI_DOUBLE, dest, 2006, MPI_COMM_WORLD);
        dest = ++dest < clSize ? dest : 1;
    }
    hasRemaining = 0;
    for (int dest = 1; dest < clSize; dest++) {
        MPI_Send(&hasRemaining, 1, MPI_INT, dest, 2004, MPI_COMM_WORLD);
    }
    fprintf(stderr, "Send Test with %d examples took \t%lfs\n", exampleCounter, ((double)(clock() - start)) / ((double)1000000));
    return exampleCounter;
}

int receiveExamples(int dimension, Model *model, int clRank) {
    Point ex;
    ex.value = malloc((dimension +1) * sizeof(double));
    ex.id = -1;
    Match match;
    int exampleCounter = 0;
    //
    MPI_Status status;
    int hasRemaining = 1;
    double *value = ex.value;
    MPI_Recv(&hasRemaining, 1, MPI_INT, MPI_ANY_SOURCE, 2004, MPI_COMM_WORLD, &status);
    clock_t start = clock();
    while (hasRemaining) {
        MPI_Recv(&ex, sizeof(Point), MPI_BYTE, MPI_ANY_SOURCE, 2005, MPI_COMM_WORLD, &status);
        MPI_Recv(value, dimension, MPI_DOUBLE, MPI_ANY_SOURCE, 2006, MPI_COMM_WORLD, &status);
        ex.value = value;
        //
        classify(dimension, model, &ex, &match);
        //
        exampleCounter++;
        //
        MPI_Recv(&hasRemaining, 1, MPI_INT, MPI_ANY_SOURCE, 2004, MPI_COMM_WORLD, &status);
    }
    fprintf(stderr, "[%d] Worker classify Test with %d examples took \t%lfs\n", clRank, exampleCounter, ((double)(clock() - start)) / ((double)1000000));
    return exampleCounter;
}

int MFOG_main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int clRank, clSize;
    MPI_Comm_size(MPI_COMM_WORLD, &clSize);
    MPI_Comm_rank(MPI_COMM_WORLD, &clRank);
    if (clRank == 0) {
        char processor_name[MPI_MAX_PROCESSOR_NAME];
        int name_len;
        MPI_Get_processor_name(processor_name, &name_len);
        fprintf(stderr, "Processor %s, Rank %d out of %d processors\n", processor_name, clRank, clSize);
    }
    if (clSize <= 1) {
        errx(EXIT_FAILURE, "Cluster with only one node.");
    }
    /*
    # Root:
        - Read Model
        - Broadcast Model
        - Read Examples
        - Start Timer
        - Send Examples Loop
        - Close/clean-up
    # Dispatcher
    # Workers:
        - Rcv Model
        - Rcv Example
        - Classify
    */
    Model model;
    model.dimension = 22;
    if (clRank == 0) {
        char *modelName = "datasets/model-clean.csv";
        readModel(model.dimension, modelName, &model);
        sendModel(model.dimension, &model, clRank, clSize);

        char *testName = "datasets/test.csv";
        Point *examples;
        examples = readExamples(model.dimension, testName);
        
        clock_t start = clock();
        printf("#id,isMach,clusterId,label,distance,radius\n");
        int exampleCounter = sendExamples(model.dimension, examples, clSize);
        
        MPI_Barrier(MPI_COMM_WORLD);
        fprintf(stderr, "Classification with %d examples took \t%lfs\n", exampleCounter, ((double)(clock() - start)) / ((double)1000000));
    } else {
        receiveModel(model.dimension, &model, clRank);
        
        receiveExamples(model.dimension, &model, clRank);
        
        MPI_Barrier(MPI_COMM_WORLD);
    }
    
    MPI_Finalize();
    return 0;
}

#ifndef MAIN
#define MAIN
int main(int argc, char **argv) {
    clock_t start = clock();
    MFOG_main(argc, argv);
    fprintf(stderr, "Done %s in \t%fs\n", argv[0], ((double)(clock() - start)) / ((double)1000000));
    return 0;
}
#endif
