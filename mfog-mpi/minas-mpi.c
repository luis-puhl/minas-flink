#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <err.h>
#include <string.h>
#include <mpi.h>

#include "./minas.h"

#define __ELAPSED__ ((double)(clock() - start)) / ((double)1000000)
#define line_len 10 * 1024

double MNS_distance(float a[], float b[], int dimension) {
    double distance = 0;
    for (int i = 0; i < dimension; i++) {
        float diff = a[i] - b[i];
        distance += diff * diff;
    }
    #ifdef SQR_DISTANCE
        return distance;
    #else
        return sqrt(distance);
    #endif // SQR_DISTANCE
}

void readModel(int dimension, char *modelName, Model *model) {
    clock_t start = clock();
    char line[line_len + 1];
    //
    model->size = 0;
    model->vals = malloc(1 * sizeof(Cluster));
    //
    FILE *file;
    fprintf(stderr, "Reading model from \t%s\n", modelName);
    file = fopen(modelName, "r");
    if (file == NULL) errx(EXIT_FAILURE, "bad file open '%s'", modelName);
    while (fgets(line, line_len, file)) {
        if (line[0] == '#') continue;
        model->vals = realloc(model->vals, (++model->size) * sizeof(Cluster));
        Cluster *cl = &(model->vals[model->size - 1]);
        cl->center = malloc(dimension * sizeof(float));
        // #id,label,category,matches,time,meanDistance,radius,c0,c1,c2,c3,c4,c5,c6,c7,c8,c9,c10,c11,c12,c13,c14,c15,c16,c17,c18,c19,c20,c21
        int assigned = sscanf(line,
            "%d,%c,%c,"
            "%d,%d,%f,%f,"
            "%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f\n",
            &cl->id, &cl->label, &cl->category,
            &cl->matches, &cl->time, &cl->meanDistance, &cl->radius,
            &cl->center[0], &cl->center[1], &cl->center[2], &cl->center[3], &cl->center[4],
            &cl->center[5], &cl->center[6], &cl->center[7], &cl->center[8], &cl->center[9],
            &cl->center[10], &cl->center[11], &cl->center[12], &cl->center[13], &cl->center[14],
            &cl->center[15], &cl->center[16], &cl->center[17], &cl->center[18], &cl->center[19],
            &cl->center[20], &cl->center[21]
        );
        if (assigned != 29) errx(EXIT_FAILURE, "File with wrong format  '%s'", modelName);
    }
    fclose(file);
    fprintf(stderr, "Read model with %d clusters took \t%lfs\n", model->size, __ELAPSED__);
}

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
            MPI_Send(cl->center, dimension, MPI_FLOAT, dest, 2003, MPI_COMM_WORLD);
        }
    }
    fprintf(stderr, "[%d] Send model with %d clusters took \t%lfs\n", clRank, model->size, __ELAPSED__);
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
        float *center = malloc(dimension * sizeof(float));
        MPI_Recv(center, dimension, MPI_FLOAT, MPI_ANY_SOURCE, 2003, MPI_COMM_WORLD, &status);
        cl->center = center;
    }
    fprintf(stderr, "[%d] Recv model with %d clusters took \t%lfs\n", clRank, model->size, __ELAPSED__);
}

Point *readExamples(int dimension, char *testName) {
    clock_t start = clock();
    char line[line_len + 1];
    //
    Point *exs;
    unsigned int exSize = 1;
    exs = malloc(exSize * sizeof(Point));
    //
    FILE *file;
    fprintf(stderr, "Reading test from %20s\n", testName);
    file = fopen(testName, "r");
    if (file == NULL) errx(EXIT_FAILURE, "bad file open '%s'", testName);
    // for (examples)
    while (fgets(line, line_len, file)) {
        if (line[0] == '#') continue;
        // 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,A
        Point *ex = &(exs[exSize-1]);
        ex->value = malloc(dimension * sizeof(float));
        int assigned = sscanf(line,
            "%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,"
            "%c\n",
            &ex->value[0], &ex->value[1], &ex->value[2], &ex->value[3], &ex->value[4],
            &ex->value[5], &ex->value[6], &ex->value[7], &ex->value[8], &ex->value[9],
            &ex->value[10], &ex->value[11], &ex->value[12], &ex->value[13], &ex->value[14],
            &ex->value[15], &ex->value[16], &ex->value[17], &ex->value[18], &ex->value[19],
            &ex->value[20], &ex->value[21], &ex->label
        );
        ex->id = exSize;
        if (assigned != 23) errx(EXIT_FAILURE, "File with wrong format  '%s'", testName);
        //
        exs = realloc(exs, (++exSize) * sizeof(Point));
    }
    fclose(file);
    Point *ex = &(exs[exSize-1]);
    ex->id = -1;
    ex->value = NULL;
    ex->label = '\0';
    fprintf(stderr, "Read test with %d examples took \t%lfs\n", exSize, __ELAPSED__);
    return exs;
}

int sendExamples(int dimension, Point *examples, int clSize) {
    int dest = 1, hasRemaining = 1, exampleCounter = 0;
    clock_t start = clock();
    for (exampleCounter = 0; examples[exampleCounter].value != NULL; exampleCounter++) {
        Point *ex = &(examples[exampleCounter]);
        MPI_Send(&hasRemaining, 1, MPI_INT, dest, 2004, MPI_COMM_WORLD);
        MPI_Send(ex, sizeof(Point), MPI_BYTE, dest, 2005, MPI_COMM_WORLD);
        MPI_Send(ex->value, dimension, MPI_FLOAT, dest, 2006, MPI_COMM_WORLD);
        dest = ++dest < clSize ? dest : 1;
    }
    hasRemaining = 0;
    for (int dest = 1; dest < clSize; dest++) {
        MPI_Send(&hasRemaining, 1, MPI_INT, dest, 2004, MPI_COMM_WORLD);
    }
    fprintf(stderr, "Send Test with %d examples took \t%lfs\n", exampleCounter, __ELAPSED__);
    return exampleCounter;
}

int receiveExamples(int dimension, Model *model, int clRank) {
    Point ex;
    ex.value = malloc((dimension +1) * sizeof(float));
    ex.id = -1;
    Match match;
    int exampleCounter = 0;
    //
    MPI_Status status;
    int hasRemaining = 1;
    float *value = ex.value;
    MPI_Recv(&hasRemaining, 1, MPI_INT, MPI_ANY_SOURCE, 2004, MPI_COMM_WORLD, &status);
    clock_t start = clock();
    while (hasRemaining) {
        MPI_Recv(&ex, sizeof(Point), MPI_BYTE, MPI_ANY_SOURCE, 2005, MPI_COMM_WORLD, &status);
        MPI_Recv(value, dimension, MPI_FLOAT, MPI_ANY_SOURCE, 2006, MPI_COMM_WORLD, &status);
        ex.value = value;
        //
        match.distance = (float) dimension;
        match.pointId = ex.id;
        float *center;
        for (int i = 0; i < model->size; i++) {
            double distance = MNS_distance(ex.value, model->vals[i].center, dimension);
            if (match.distance > distance) {
                match.clusterId = model->vals[i].id;
                match.label = model->vals[i].label;
                match.radius = model->vals[i].radius;
                match.distance = distance;
            }
        }
        match.isMatch = match.distance <= match.radius ? 'y' : 'n';
        
        printf("%d,%c,%d,%c,%lf,%lf\n",
            match.pointId, match.isMatch, match.clusterId,
            match.label, match.distance, match.radius
        );
        exampleCounter++;
        //
        MPI_Recv(&hasRemaining, 1, MPI_INT, MPI_ANY_SOURCE, 2004, MPI_COMM_WORLD, &status);
    }
    fprintf(stderr, "[%d] Worker classify Test with %d examples took \t%lfs\n", clRank, exampleCounter, __ELAPSED__);
    return exampleCounter;
}

int main(int argc, char **argv) {
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
        fprintf(stderr, "Classification with %d examples took \t%lfs\n", exampleCounter, __ELAPSED__);
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
