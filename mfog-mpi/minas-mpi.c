#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <err.h>
#include <string.h>
#include <mpi.h>

#include "./minas.h"

void sendModel(int dimension, Model *model, int clRank, int clSize) {
    // MPI_Broadcast
    // MPI_Pack
    clock_t start = clock();
    for (int dest = 1; dest < clSize; dest++) {
        // fprintf(stderr, "[%d] Sending to %d\n", clRank, dest);
        MPI_Send(model, sizeof(Model), MPI_BYTE, dest, 2000, MPI_COMM_WORLD);
        for (int i = 0; i < model->size; i++) {
            Cluster *cl = &(model->vals[i]);
            MPI_Send(cl, sizeof(Cluster), MPI_BYTE, dest, 2002, MPI_COMM_WORLD);
            MPI_Send(cl->center, dimension, MPI_DOUBLE, dest, 2003, MPI_COMM_WORLD);
        }
    }
    fprintf(stderr, "[%d] Send model with %d clusters took \t%lfs\n", clRank, model->size, ((double)(clock() - start)) / ((double)1000000));
}

void receiveModel(int dimension, Model *model, int clRank) {
    clock_t start = clock();
    // fprintf(stderr, "[%d] Waiting for model\n", clRank);
    MPI_Status status;
    MPI_Recv(model, sizeof(Model), MPI_BYTE, MPI_ANY_SOURCE, 2000, MPI_COMM_WORLD, &status);
    model->vals = malloc(model->size * sizeof(Cluster));
    // fprintf(stderr, "[%d] Model size to recv %d\n", clRank, model->size);
    for (int i = 0; i < model->size; i++) {
        Cluster *cl = &(model->vals[i]);
        MPI_Recv(cl, sizeof(Cluster), MPI_BYTE, MPI_ANY_SOURCE, 2002, MPI_COMM_WORLD, &status);
        double *center = malloc(dimension * sizeof(double));
        MPI_Recv(center, dimension, MPI_DOUBLE, MPI_ANY_SOURCE, 2003, MPI_COMM_WORLD, &status);
        cl->center = center;
    }
    fprintf(stderr, "[%d] Recv model with %d clusters took \t%lfs\n", clRank, model->size, ((double)(clock() - start)) / ((double)1000000));
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
    //
    int dimension = 22;

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
    if (clRank == 0) {
        Model model;
        char *modelName = "datasets/model-clean.csv";
        readModel(dimension, modelName, &model);
        sendModel(dimension, &model, clRank, clSize);

        char *testName = "datasets/test.csv";
        Point *examples;
        examples = readExamples(dimension, testName);
        
        clock_t start = clock();
        printf("#id,isMach,clusterId,label,distance,radius\n");
        int exampleCounter = sendExamples(dimension, examples, clSize);
        
        int eofMarker;
        MPI_Status status;
        for (int dest = 1; dest < clSize; dest++) {
            MPI_Recv(&eofMarker, 1, MPI_INT, MPI_ANY_SOURCE, 2004, MPI_COMM_WORLD, &status);
        }
        fprintf(stderr, "Classification with %d examples took \t%lfs\n", exampleCounter, ((double)(clock() - start)) / ((double)1000000));
    } else {
        Model model;
        receiveModel(dimension, &model, clRank);
        
        receiveExamples(dimension, &model, clRank);

        int eofMarker = EOF;
        MPI_Send(&eofMarker, 1, MPI_INT, 0, 2004, MPI_COMM_WORLD);
    }
    
    MPI_Finalize();
    return 0;
}

#ifndef MAIN
#define MAIN
int main(int argc, char **argv) {
    MFOG_main(argc, argv);
    return 0;
}
#endif
