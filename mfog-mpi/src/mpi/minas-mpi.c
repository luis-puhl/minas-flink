#ifndef MFOG_C
#define MFOG_C

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <err.h>
#include <string.h>
#include <mpi.h>

#include "../minas/minas.h"
#include "../util/loadenv.h"

#define MFOG_MASTER_RANK 0
#define DEBUG_LN
#ifdef DEBUG
#define DEBUG_LN fprintf(stderr, "%d %s\n", __LINE__, __FUNCTION__); fflush(stderr);
#endif
#define MPI_RETURN if (mpiReturn != MPI_SUCCESS) { MPI_Abort(MPI_COMM_WORLD, mpiReturn); errx(EXIT_FAILURE, "MPI Abort %d\n", mpiReturn); }

void sendModel(int dimension, Model *model, int clRank, int clSize, FILE *timing, char *executable) {
    int mpiReturn;
    clock_t start = clock();
    int bufferSize = sizeof(Model) +
        (model->size) * sizeof(Cluster) +
        dimension * (model->size) * sizeof(double);
    char *buffer = malloc(bufferSize);
    int position = 0;
    DEBUG_LN mpiReturn = MPI_Pack(model, sizeof(Model), MPI_BYTE, buffer, bufferSize, &position, MPI_COMM_WORLD);
    MPI_RETURN
    DEBUG_LN mpiReturn = MPI_Pack(model->vals, model->size * sizeof(Cluster), MPI_BYTE, buffer, bufferSize, &position, MPI_COMM_WORLD);
    MPI_RETURN
    for (int i = 0; i < model->size; i++) {
        mpiReturn = MPI_Pack(model->vals[i].center, dimension, MPI_DOUBLE, buffer, bufferSize, &position, MPI_COMM_WORLD);
        MPI_RETURN
    }
    DEBUG_LN if (position != bufferSize) errx(EXIT_FAILURE, "Buffer sizing error. Used %d of %d.\n", position, bufferSize);
    DEBUG_LN mpiReturn = MPI_Bcast(&bufferSize, 1, MPI_INT, MFOG_MASTER_RANK, MPI_COMM_WORLD);
    MPI_RETURN
    DEBUG_LN mpiReturn = MPI_Bcast(buffer, position, MPI_PACKED, MFOG_MASTER_RANK, MPI_COMM_WORLD);
    MPI_RETURN
    DEBUG_LN 
    free(buffer);
    PRINT_TIMING(timing, executable, clSize, start, model->size);
}

void receiveModel(int dimension, Model *model, int clRank) {
    clock_t start = clock();
    int bufferSize, mpiReturn;
    DEBUG_LN mpiReturn = MPI_Bcast(&bufferSize, 1, MPI_INT, MFOG_MASTER_RANK, MPI_COMM_WORLD);
    MPI_RETURN
    char *buffer = malloc(bufferSize);
    DEBUG_LN mpiReturn = MPI_Bcast(buffer, bufferSize, MPI_PACKED, MFOG_MASTER_RANK, MPI_COMM_WORLD);
    MPI_RETURN

    int position = 0;
    DEBUG_LN mpiReturn = MPI_Unpack(buffer, bufferSize, &position, model, sizeof(Model), MPI_BYTE, MPI_COMM_WORLD);
    MPI_RETURN
    model->vals = malloc(model->size * sizeof(Cluster));
    DEBUG_LN mpiReturn = MPI_Unpack(buffer, bufferSize, &position, model->vals, model->size * sizeof(Cluster), MPI_BYTE, MPI_COMM_WORLD);
    MPI_RETURN
    for (int i = 0; i < model->size; i++) {
        model->vals[i].center = malloc(model->dimension * sizeof(double));
        mpiReturn = MPI_Unpack(buffer, bufferSize, &position, model->vals[i].center, model->dimension, MPI_DOUBLE, MPI_COMM_WORLD);
        MPI_RETURN
    }
    free(buffer);
    fprintf(stderr, "[%d] Recv model with %d clusters took \t%es\n", clRank, model->size, ((double)(clock() - start)) / ((double)1000000));
}

int receiveClassifications(FILE *matches) {
    int hasMessage = 0, matchesCounter = 0;
    Match match;
    MPI_Iprobe(MPI_ANY_SOURCE, 2005, MPI_COMM_WORLD, &hasMessage, MPI_STATUS_IGNORE);
    while (hasMessage) {
        MPI_Recv(&match, sizeof(Match), MPI_BYTE, MPI_ANY_SOURCE, 2005, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        fprintf(matches, "%d,%c,%d,%c,%e,%e\n",
                // match.pointId, match.isMatch, match.cluster->id,
                // match.cluster->label, match.distance, match.cluster->radius
                match.pointId, match.isMatch, match.clusterId,
                match.label, match.distance, match.radius
        );
        matchesCounter++;
        MPI_Iprobe(MPI_ANY_SOURCE, 2005, MPI_COMM_WORLD, &hasMessage, MPI_STATUS_IGNORE);
    }
    return matchesCounter;
}

int sendExamples(int dimension, Point *examples, int clSize, FILE *matches, FILE *timing, char *executable) {
    int dest = 1, exampleCounter = 0;
    clock_t start = clock();
    int bufferSize = sizeof(Point) + dimension * sizeof(double);
    char *buffer = malloc(bufferSize);
    MPI_Bcast(&bufferSize, 1, MPI_INT, MFOG_MASTER_RANK, MPI_COMM_WORLD);
    //
    Match match;

    for (exampleCounter = 0; examples[exampleCounter].value != NULL; exampleCounter++) {
        Point *ex = &(examples[exampleCounter]);
        int position = 0;
        MPI_Pack(ex, sizeof(Point), MPI_BYTE, buffer, bufferSize, &position, MPI_COMM_WORLD);
        MPI_Pack(ex->value, dimension, MPI_DOUBLE, buffer, bufferSize, &position, MPI_COMM_WORLD);
        MPI_Send(buffer, position, MPI_PACKED, dest, 2004, MPI_COMM_WORLD);
        //
        receiveClassifications(matches);
        //
        dest = ++dest < clSize ? dest : 1;
    }
    Point ex;
    ex.id = -1;
    ex.value = malloc(dimension * sizeof(double));
    for (int dest = 1; dest < clSize; dest++) {
        int position = 0;
        MPI_Pack(&ex, sizeof(Point), MPI_BYTE, buffer, bufferSize, &position, MPI_COMM_WORLD);
        MPI_Pack(ex.value, dimension, MPI_DOUBLE, buffer, bufferSize, &position, MPI_COMM_WORLD);
        MPI_Send(buffer, position, MPI_PACKED, dest, 2004, MPI_COMM_WORLD);
    }
    MPI_Barrier(MPI_COMM_WORLD);
    receiveClassifications(matches);
    free(buffer);
    free(ex.value);
    PRINT_TIMING(timing, executable, clSize, start, exampleCounter);
    return exampleCounter;
}

int receiveExamples(int dimension, Model *model, int clRank) {
    Point ex;
    double *valuePtr = malloc((dimension + 1) * sizeof(double));
    ex.id = 0;
    Match match;
    int exampleCounter = 0;
    //
    int bufferSize;
    MPI_Bcast(&bufferSize, 1, MPI_INT, MFOG_MASTER_RANK, MPI_COMM_WORLD);
    char *buffer = malloc(bufferSize);
    //
    clock_t start = clock();
    while (ex.id >= 0) {
        MPI_Recv(buffer, bufferSize, MPI_PACKED, MPI_ANY_SOURCE, 2004, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        int position = 0;
        MPI_Unpack(buffer, bufferSize, &position, &ex, sizeof(Point), MPI_BYTE, MPI_COMM_WORLD);
        if (ex.id < 0) break;
        MPI_Unpack(buffer, bufferSize, &position, valuePtr, dimension, MPI_DOUBLE, MPI_COMM_WORLD);
        ex.value = valuePtr;
        //
        classify(dimension, model, &ex, &match);
        MPI_Send(&match, sizeof(Match), MPI_BYTE, MFOG_MASTER_RANK, 2005, MPI_COMM_WORLD);
        //
        exampleCounter++;
    }
    fprintf(stderr, "[%d] Worker classify Test with %d examples took \t%lfs\n", clRank, exampleCounter, ((double)(clock() - start)) / ((double)1000000));
    MPI_Barrier(MPI_COMM_WORLD);
    // match.pointId = -1;
    // MPI_Send(&match, sizeof(Match), MPI_BYTE, MFOG_MASTER_RANK, 2005, MPI_COMM_WORLD);
    free(buffer);
    free(valuePtr);
    return exampleCounter;
}

int MNS_mfog_main(int argc, char *argv[], char **envp) {
    int mpiReturn;
    mpiReturn = MPI_Init(&argc, &argv);
    MPI_RETURN
    int clRank, clSize;
    mpiReturn = MPI_Comm_size(MPI_COMM_WORLD, &clSize);
    mpiReturn = MPI_Comm_rank(MPI_COMM_WORLD, &clRank);
    MPI_RETURN
    if (clRank == 0) {
        char processor_name[MPI_MAX_PROCESSOR_NAME];
        int name_len;
        mpiReturn = MPI_Get_processor_name(processor_name, &name_len);
        MPI_RETURN
        printEnvs(argc, argv, envp);
        fprintf(stderr, "Processor %s, Rank %d out of %d processors\n", processor_name, clRank, clSize);
        //
        #ifdef MPI_VERSION
        fprintf(stderr, "MPI %d %d\n", MPI_VERSION, MPI_SUBVERSION);
        #endif // MPI_VERSION
        //
        char value[140]; int flag;
        //
        mpiReturn = MPI_Info_get(MPI_INFO_ENV, "arch", 140, value, &flag);
        MPI_RETURN
        if (flag) fprintf(stderr, "MPI arch = %s\n", value);
        //
        mpiReturn = MPI_Info_get(MPI_INFO_ENV, "host", 140, value, &flag);
        MPI_RETURN
        if (flag) fprintf(stderr, "MPI host = %s\n", value);
        //
        mpiReturn = MPI_Info_get(MPI_INFO_ENV, "thread_level", 140, value, &flag);
        MPI_RETURN
        if (flag) fprintf(stderr, "MPI thread_level = %s\n", value);
    }
    if (clSize <= 1) {
        fprintf(stderr, "Cluster with only one node. Running serial code.\n");
        MNS_minas_main(argc, argv, envp);
        MPI_Finalize();
        return 0;
    }
    //
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
        char *executable = argv[0]; 
        char *modelCsv, *examplesCsv, *matchesCsv, *timingLog;
        FILE *modelFile, *examplesFile, *matches, *timing;
        #define VARS_SIZE 4
        char *varNames[] = { "MODEL_CSV", "EXAMPLES_CSV", "MATCHES_CSV", "TIMING_LOG"};
        char **fileNames[] = { &modelCsv, &examplesCsv, &matchesCsv, &timingLog };
        FILE **files[] = { &modelFile, &examplesFile, &matches, &timing };
        char *fileModes[] = { "r", "r", "w", "a" };
        loadEnv(argc, argv, envp, VARS_SIZE, varNames, fileNames, files, fileModes);
        printf(
            "Reading examples from  '%s'\n"
            "Reading model from     '%s'\n"
            "Writing matches to     '%s'\n"
            "Writing timing to      '%s'\n",
            examplesCsv, modelCsv, matchesCsv, timingLog
        );
        
        readModel(model.dimension, modelFile, &model, timing, executable);
        Point *examples;
        int nExamples;
        examples = readExamples(model.dimension, examplesFile, &nExamples, timing, executable);
        //
        sendModel(model.dimension, &model, clRank, clSize, timing, executable);

        clock_t start = clock();
        fprintf(matches, "#id,isMach,clusterId,label,distance,radius\n");
        int exampleCounter = sendExamples(model.dimension, examples, clSize, matches, timing, executable);

        MPI_Barrier(MPI_COMM_WORLD);
        PRINT_TIMING(timing, executable, clSize, start, exampleCounter);
        closeEnv(VARS_SIZE, varNames, fileNames, files, fileModes);
    } else {
        receiveModel(model.dimension, &model, clRank);

        receiveExamples(model.dimension, &model, clRank);

        MPI_Barrier(MPI_COMM_WORLD);
    }
    
    MPI_Finalize();
    return 0;
}

#endif // MFOG_C
