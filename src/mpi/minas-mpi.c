#ifndef MFOG_C
#define MFOG_C

#include "./minas-mpi.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <err.h>
#include <string.h>
#include <mpi.h>

#include "../baseline/base.h"

// #include "../minas/minas.h"
// #include "../util/loadenv.h"

void sendModel(Params* params, Model *model, int clRank, int clSize, FILE *timing, char *executable) {
    int mpiReturn;
    clock_t start = clock();
    int bufferSize = sizeof(Model) +
        (model->size) * sizeof(Cluster) +
        params->dim * (model->size) * sizeof(double);
    char *buffer = malloc(bufferSize);
    int position = 0;
    mpiReturn = MPI_Pack(model, sizeof(Model), MPI_BYTE, buffer, bufferSize, &position, MPI_COMM_WORLD);
    if (mpiReturn != MPI_SUCCESS) {
        MPI_Abort(MPI_COMM_WORLD, mpiReturn);
        errx(EXIT_FAILURE, "MPI Abort %d\n", mpiReturn);
    }
    mpiReturn = MPI_Pack(model->clusters, model->size * sizeof(Cluster), MPI_BYTE, buffer, bufferSize, &position, MPI_COMM_WORLD);
    MPI_RETURN
    for (int i = 0; i < model->size; i++) {
        mpiReturn = MPI_Pack(model->clusters[i].center, params->dim, MPI_DOUBLE, buffer, bufferSize, &position, MPI_COMM_WORLD);
        MPI_RETURN
    }
    if (position != bufferSize) errx(EXIT_FAILURE, "Buffer sizing error. Used %d of %d.\n", position, bufferSize);
    mpiReturn = MPI_Bcast(&bufferSize, 1, MPI_INT, MFOG_MASTER_RANK, MPI_COMM_WORLD);
    MPI_RETURN
    mpiReturn = MPI_Bcast(buffer, position, MPI_PACKED, MFOG_MASTER_RANK, MPI_COMM_WORLD);
    MPI_RETURN
    
    free(buffer);
    PRINT_TIMING(timing, executable, clSize, start, model->size);
}

void receiveModel(Params* params, Model *model, int clRank) {
    clock_t start = clock();
    int bufferSize, mpiReturn;
    mpiReturn = MPI_Bcast(&bufferSize, 1, MPI_INT, MFOG_MASTER_RANK, MPI_COMM_WORLD);
    MPI_RETURN
    char *buffer = malloc(bufferSize);
    mpiReturn = MPI_Bcast(buffer, bufferSize, MPI_PACKED, MFOG_MASTER_RANK, MPI_COMM_WORLD);
    MPI_RETURN

    int position = 0;
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
    free(buffer);
    fprintf(stderr, "[%d] Recv model with %d clusters took \t%es\n", clRank, model->size, ((double)(clock() - start)) / ((double)1000000));
}

int receiveClassifications(Match *memMatches) {
    int hasMessage = 0, matchesCounter = 0;
    Match match;
    MPI_Iprobe(MPI_ANY_SOURCE, 2005, MPI_COMM_WORLD, &hasMessage, MPI_STATUS_IGNORE);
    while (hasMessage) {
        MPI_Recv(&match, sizeof(Match), MPI_BYTE, MPI_ANY_SOURCE, 2005, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        memMatches[match.pointId] = match;
        // fprintf(matches, MATCH_CSV_LINE_FORMAT, MATCH_CSV_LINE_PRINT_ARGS(match));
        matchesCounter++;
        MPI_Iprobe(MPI_ANY_SOURCE, 2005, MPI_COMM_WORLD, &hasMessage, MPI_STATUS_IGNORE);
    }
    return matchesCounter;
}

int sendExamples(int dimension, Example examples[], Match memMatches[], int clSize, FILE *timing, char *executable) {
    int dest = 1, exampleCounter = 0;
    clock_t start = clock();
    int bufferSize = sizeof(Example) + dimension * sizeof(double);
    char *buffer = malloc(bufferSize);
    MPI_Bcast(&bufferSize, 1, MPI_INT, MFOG_MASTER_RANK, MPI_COMM_WORLD);
    //
    //Match match;

    for (exampleCounter = 0; examples[exampleCounter].value != NULL; exampleCounter++) {
        Example *ex = &(examples[exampleCounter]);
        int position = 0;
        MPI_Pack(ex, sizeof(Example), MPI_BYTE, buffer, bufferSize, &position, MPI_COMM_WORLD);
        MPI_Pack(ex->value, dimension, MPI_DOUBLE, buffer, bufferSize, &position, MPI_COMM_WORLD);
        MPI_Send(buffer, position, MPI_PACKED, dest, 2004, MPI_COMM_WORLD);
        //
        receiveClassifications(memMatches);
        //
        dest = ++dest < clSize ? dest : 1;
    }
    Example ex;
    ex.id = -1;
    ex.value = malloc(dimension * sizeof(double));
    for (int dest = 1; dest < clSize; dest++) {
        int position = 0;
        // TODO: enviar apenas o point com id=-1, o valor não é utilizado
        MPI_Pack(&ex, sizeof(Example), MPI_BYTE, buffer, bufferSize, &position, MPI_COMM_WORLD);
        MPI_Pack(ex.value, dimension, MPI_DOUBLE, buffer, bufferSize, &position, MPI_COMM_WORLD);
        MPI_Send(buffer, position, MPI_PACKED, dest, 2004, MPI_COMM_WORLD);
    }
    MPI_Barrier(MPI_COMM_WORLD);
    receiveClassifications(memMatches);
    free(buffer);
    free(ex.value);
    PRINT_TIMING(timing, executable, clSize, start, exampleCounter);
    return exampleCounter;
}

int receiveExamples(int dimension, Model *model, int clRank) {
    Example ex;
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
    // double *distances = malloc(model->size * sizeof(double));
    while (ex.id >= 0) {
        MPI_Recv(buffer, bufferSize, MPI_PACKED, MPI_ANY_SOURCE, 2004, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        int position = 0;
        MPI_Unpack(buffer, bufferSize, &position, &ex, sizeof(Example), MPI_BYTE, MPI_COMM_WORLD);
        if (ex.id < 0) break;
        MPI_Unpack(buffer, bufferSize, &position, valuePtr, dimension, MPI_DOUBLE, MPI_COMM_WORLD);
        ex.val = valuePtr;
        //
        identify(params, model, &ex, &match);
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

/*
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
    / *
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
    * /
    Model *model;
    int dimension = 22;
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

        model = readModel(dimension, modelFile, timing, executable);
        Example *examples;
        int nExamples;
        examples = readExamples(dimension, examplesFile, &nExamples, timing, executable);
        Match *memMatches = malloc(nExamples * sizeof(Match));
        //
        sendModel(dimension, model, clRank, clSize, timing, executable);

        clock_t start = clock();
        // fprintf(matches, MATCH_CSV_HEADER);
        int exampleCounter = sendExamples(dimension, examples, memMatches, clSize, timing, executable);

        MPI_Barrier(MPI_COMM_WORLD);
        PRINT_TIMING(timing, executable, clSize, start, exampleCounter);
        closeEnv(VARS_SIZE, varNames, fileNames, files, fileModes);
    } else {
        model = malloc(sizeof(Model));
        receiveModel(dimension, model, clRank);

        receiveExamples(dimension, model, clRank);

        MPI_Barrier(MPI_COMM_WORLD);
    }
    
    MPI_Finalize();
    return 0;
}
*/

// void initMPI(int argc, char *argv[], char **envp, mfog_params_t *params) {
//     int mpiReturn;
//     mpiReturn = MPI_Init(&argc, &argv);
//     if (mpiReturn != MPI_SUCCESS) {
//         MPI_Abort(MPI_COMM_WORLD, mpiReturn);
//         errx(EXIT_FAILURE, "MPI Abort %d\n", mpiReturn);
//     }
//     int mpiRank, mpiSize;
//     mpiReturn = MPI_Comm_size(MPI_COMM_WORLD, &mpiSize);
//     mpiReturn = MPI_Comm_rank(MPI_COMM_WORLD, &mpiRank);
//     if (mpiReturn != MPI_SUCCESS) {
//         MPI_Abort(MPI_COMM_WORLD, mpiReturn);
//         errx(EXIT_FAILURE, "MPI Abort %d\n", mpiReturn);
//     }
//     printf("MPI rank / size => %d/%d\n", mpiRank, mpiSize);
//     params->mpiRank = mpiRank;
//     params->mpiSize = mpiSize;
// }

#endif // MFOG_C
