#ifndef _BASE_C
#define _BASE_C 1

#include <stdio.h>
#include <stdlib.h>
// #include <string.h>
#include <err.h>
// #include <assert.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#ifdef USE_MPI
#include <mpi.h>
#endif // USE_MPI

#include "./base.h"

char *printableLabel(char label) {
    if (isalpha(label) || label == '-') {
        char *ret = calloc(2, sizeof(char));
        ret[0] = label;
        return ret;
    }
    char *ret = calloc(20, sizeof(char));
    sprintf(ret, "%d", label);
    return ret;
}

double euclideanSqrDistance(unsigned int dim, double a[], double b[]) {
    double distance = 0;
    for (size_t d = 0; d < dim; d++) {
        distance += (a[d] - b[d]) * (a[d] - b[d]);
    }
    return distance;
}

double euclideanDistance(unsigned int dim, double a[], double b[]) {
    return sqrt(euclideanSqrDistance(dim, a, b));
}

int addTimeLog(Params *params, size_t functionPtr, clock_t time, size_t setSize, char *functionName, char *functionFile, int functionLine) {
    TimingLog *log = params->log;
    for (size_t i = 0; i < log->len; i++) {
        if (log->listHead[i].functionPtr == functionPtr) {
            log->listHead[i].setSize += setSize;
            log->listHead[i].time += time;
            log->listHead[i].calls++;
            return i;
        }
    }
    log->listHead[log->len].calls = 0;
    log->listHead[log->len].setSize = setSize;
    log->listHead[log->len].time = time;
    //
    log->listHead[log->len].functionPtr = functionPtr;
    log->listHead[log->len].functionLine = functionLine;
    log->listHead[log->len].functionName = functionName;
    log->listHead[log->len].functionFile = functionFile;
    //
    log->len++;
    if (log->len == log->maxLen) {
        log->maxLen *= 1.2;
        log->listHead = realloc(log->listHead, log->maxLen * sizeof(TimingLogEntry));
    }
    return log->len;
}

int printTimeLog(Params *params) {
    for (size_t i = 0; i < params->log->len; i++) {
        TimingLogEntry *e = &params->log->listHead[i];
        fprintf(stderr, "[%s] %le seconds for %s(%lu). At %s:%d\n",
            params->executable, ((double)e->time) / 1000000.0, e->functionName, e->setSize, e->functionFile, e->functionLine);
    }
    return params->log->len;
}

typedef struct {
    char *name, *format;
    void *ptr;
} Param;

int getParam(FILE* paramFile, const char* paramName, const char* paramFormat, int* paramVal, int required) {
    int assigned = 0;
    char printFormat[140];
    //
    char* strVal = getenv(paramName);
    if (strVal != NULL) {
        assigned += sscanf(strVal, paramFormat, paramVal);
    }
    //
    if (paramFile != NULL && (required && !assigned)) {
        sprintf(printFormat, "%s=%s\n", paramName, paramFormat);
        assigned += fscanf(paramFile, printFormat, paramVal);
    }
    //
    if (required && !assigned) {
        errx(EXIT_FAILURE, "Could not find parameter %s in env or input config file.\n", paramName);
    }
    return assigned;
}

Params* setup(int argc, char const *argv[], char *env[]) {
    // clock_t start = clock();
    FILE *paramsFile;
    if (argc == 2) {
        fprintf(stderr, "reading from file %s\n", argv[1]);
        paramsFile = fopen(argv[1], "r");
    } else {
        paramsFile = stdin;
    }
    // for (size_t i = 0; env[i] != NULL; i++) {
    //     fprintf(stderr, "env[%lu]=%s\n", i, env[i]);
    // }
    Params *params = calloc(1, sizeof(Params));
    params->executable = argv[0];
    //
    getParam(paramsFile, "k",                                   "%d",   (void*) &(params->k), 1);
    getParam(paramsFile, "dim",                                 "%d",   (void*) &(params->dim), 1);
    getParam(paramsFile, "precision",                           "%le",  (void*) &(params->precision), 1);
    getParam(paramsFile, "radiusF",                             "%le",  (void*) &(params->radiusF), 1);
    getParam(paramsFile, "minExamplesPerCluster",               "%u",   (void*) &(params->minExamplesPerCluster), 1);
    getParam(paramsFile, "noveltyF",                            "%le",  (void*) &(params->noveltyF), 1);
    //
    getParam(paramsFile, "useCluStream",                        "%u",   (void*) &(params->useCluStream), 1);
    getParam(paramsFile, "cluStream_q_maxMicroClusters",        "%u",   (void*) &(params->cluStream_q_maxMicroClusters), 1);
    getParam(paramsFile, "cluStream_time_threshold_delta_δ",    "%lf",  (void*) &(params->cluStream_time_threshold_delta_δ), 1);
    //
    getParam(paramsFile, "useRedis",                            "%u",   (void*) &(params->useRedis), 1);
    getParam(paramsFile, "useMPI",                              "%u",   (void*) &(params->useMPI), 1);
    getParam(paramsFile, "useInlineND",                         "%u",   (void*) &(params->useInlineND), 1);
    getParam(paramsFile, "useReclassification",                 "%u",   (void*) &(params->useReclassification), 1);
    // getParam(paramsFile, "remoteRedis",                         "%s",   (void*) &(params->remoteRedis), 0);
    // params->remoteRedis = "ec2-18-191-2-174.us-east-2.compute.amazonaws.com";
    params->remoteRedis = "localhost";
    //
    params->log = calloc(1, sizeof(TimingLog));
    params->log->len = 0;
    params->log->maxLen = 10;
    params->log->listHead = calloc(params->log->maxLen, sizeof(TimingLogEntry));
    #ifdef USE_MPI
    int mpiReturn;
    if (params->useMPI) {
        mpiReturn = MPI_Init(&argc, (char ***)&argv);
        if (mpiReturn != MPI_SUCCESS) {
            MPI_Abort(MPI_COMM_WORLD, mpiReturn);
            errx(EXIT_FAILURE, "MPI Abort %d\n", mpiReturn);
        }
    }
    #endif // USE_MPI
    //
    fprintf(stderr, "%s\n", params->executable);
    fprintf(stderr, "\tk" "="                                   "%d" "\n",      params->k);
    fprintf(stderr, "\tdim" "="                                 "%d" "\n",      params->dim);
    fprintf(stderr, "\tprecision" "="                           "%le" "\n",     params->precision);
    fprintf(stderr, "\tradiusF" "="                             "%le" "\n",     params->radiusF);
    fprintf(stderr, "\tminExamplesPerCluster" "="               "%u" "\n",      params->minExamplesPerCluster);
    fprintf(stderr, "\tnoveltyF" "="                            "%le" "\n",     params->noveltyF);
    fprintf(stderr, "\tuseCluStream" "="                        "%u" "\n",      params->useCluStream);
    fprintf(stderr, "\tcluStream_q_maxMicroClusters" "="        "%u" "\n",      params->cluStream_q_maxMicroClusters);
    fprintf(stderr, "\tcluStream_time_threshold_delta_δ" "="    "%lf" "\n",     params->cluStream_time_threshold_delta_δ);
    fprintf(stderr, "\tremoteRedis" "="                         "%s" "\n",      params->remoteRedis);
    fprintf(stderr, "\tuseMPI" "="                              "%u" "\n",      params->useMPI);
    fprintf(stderr, "\tuseInlineND" "="                         "%u" "\n",      params->useInlineND);
    fprintf(stderr, "\tuseReclassification" "="                 "%u" "\n",      params->useReclassification);
    // fprintf(stderr, "setup done\n");
    return params;
}

void tearDown(int argc, char const *argv[], char *env[], Params *params) {
    #ifdef USE_MPI
    int mpiReturn;
    if (params->useMPI) {
        mpiReturn = MPI_Finalize();
        if (mpiReturn != MPI_SUCCESS) {
            errx(EXIT_FAILURE, "MPI Abort %d\n", mpiReturn);
        }
    }
    #endif // USE_MPI
}

#endif // !_BASE_C
