#ifndef _LOAD_ENV_C
#define _LOAD_ENV_C 1

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <err.h>
#include <string.h>

#include "./loadenv.h"

#define PRINT_ERROR fprintf(stderr, " At "__FILE__":%d\n", __LINE__);

char *paramEqualsOrNext(char *varName, char *envOrArg, char *nextArg) {
    if (varName == NULL) return NULL;
    int diff = 0, i;
    for (i = 0; varName[i] != '\0' && diff == 0; i++) diff += varName[i] - envOrArg[i];
    if (diff != 0) return 0;
    if (envOrArg[i] == '=') {
        return &(envOrArg[i + 1]);
    } else if (envOrArg[i] == '\0') {
        return nextArg;
    }
    return NULL;
}

char* findEnvVar(int argc, char *argv[], char **envp, char *varName) {
    const char *prefixMfog = "MFOG_";
    for (char **env = envp; *env != 0; env++) {
        char *thisEnv = *env;
        int diff = 0, i = 0;
        for (; prefixMfog[i] != '\0' && diff == 0; i++) diff += prefixMfog[i] - thisEnv[i];
        //
        if (diff != 0) continue;
        char *strVarPtr = paramEqualsOrNext(varName, &(thisEnv[i]), NULL);
        if (strVarPtr != 0) {
            return strVarPtr;
        }
    }
    for (int arg = 1; arg < argc; arg++) {
        char *strVarPtr = paramEqualsOrNext(varName, argv[arg], argv[arg + 1]);
        if (strVarPtr != 0) {
            return strVarPtr;
        }
    }
    // fprintf(stderr, "Parameter '%s' not found in args or evn.", varName);PRINT_ERROR
    return NULL;
}

int findEnvFlag(int argc, char *argv[], char **envp, char *varName) {
    const char *prefixMfog = "MFOG_";
    for (char **env = envp; *env != 0; env++) {
        char *thisEnv = *env;
        int diff = 0, i = 0;
        for (; prefixMfog[i] != '\0' && diff == 0; i++) diff += prefixMfog[i] - thisEnv[i];
        if (diff != 0) continue;
        for (; varName[i] != '\0' && diff == 0; i++) diff += varName[i] - thisEnv[i];
        if (diff != 0) continue;
        return 1;
    }
    for (int arg = 1; arg < argc; arg++) {
        int diff = 0, i = 0;
        for (; varName[i] != '\0' && diff == 0; i++) diff += varName[i] - argv[arg][i];
        if (diff != 0) continue;
        return 1;
    }
    // fprintf(stderr, "Parameter '%s' not found in args or env.", varName);PRINT_ERROR
    return 0;
}

#define IS_STDOUT 'o'
#define IS_STDERR 'e'
char isStdFile(char fileName[]) {
    if (fileName == NULL) {
        PRINT_ERROR
        return '\0';
    }
    const char *stdoutName = "stdout";
    const char *stderrName = "stderr";
    int isStdout = 0, isStderr = 0, i;
    for (i = 0; stdoutName[i] != '\0'; i++) {
        isStdout += stdoutName[i] - fileName[i];
        isStderr += stderrName[i] - fileName[i];
        if (fileName[i] == '\0') break;
    }
    if (isStdout == 0) {
        return IS_STDOUT;
    }
    if (isStdout == 0) {
        return IS_STDERR;
    }
    return '\0';
}

FILE *loadFileFromEnvValue(char varName[], char fileName[], char fileMode[]) {
    if (fileName == NULL || fileMode == NULL) {
        fprintf(stderr, "Expected argument '%s' set to '%s' to be '%s'-able file.", varName, fileName, fileMode);PRINT_ERROR
        return NULL;
    }
    //
    FILE *file;
    switch (isStdFile(fileName)) {
    case IS_STDOUT:
        file = stdout;
        break;
    case IS_STDERR:
        file = stderr;
        break;
    
    default:
        file = fopen(fileName, fileMode);
        break;
    }
    if (file == NULL) {
        fprintf(stderr, "Expected argument '%s' set to '%s' to be '%s'-able file.", varName, fileName, fileMode);PRINT_ERROR
    }
    return file;
}

FILE *loadEnvFile(int argc, char *argv[], char **envp, char varName[], char **fileName, FILE **file, char fileMode[]) {
    char *varValue = findEnvVar(argc, argv, envp, varName);
    if (varValue == NULL) {
        if (*file != NULL) {
            // fprintf(stderr, "Using default value for param '%s' with value '%s' (%p).", varName, "\0", *file);PRINT_ERROR
        } else if (*fileName != NULL) {
            *(file) = loadFileFromEnvValue(varName, *fileName, fileMode);
            // fprintf(stderr, "Using default string for param '%s' with value '%s' (%p).", varName, *fileName, *file);PRINT_ERROR
        } else if (fileMode[0] == 'w' || fileMode[0] == 'a') {
            // fprintf(stderr, "Expected argument or environment '%s' to be defined and '%s'-able."
            //     "\tWill use stdout as append file.", varName, fileMode);
            // PRINT_ERROR
            *fileName = "stdout";
            *(file) = stdout;
        }
    } else {
        *fileName = varValue;
        *(file) = loadFileFromEnvValue(varName, *fileName, fileMode);
    }
    return *(file);
}

int loadEnvVar(int argc, char *argv[], char **envp, char varType, char *varName, char **strVarPtr, int *valuePtr) {
    char *varValue = findEnvVar(argc, argv, envp, varName);
    if (varValue == NULL) {
        return 1;
    }
    *strVarPtr = varValue;
    switch (varType) {
    case 'i':
        *(valuePtr) = atoi(*strVarPtr);
        break;
    case 'f':
        *(valuePtr) = atof(*strVarPtr);
        break;
    
    default:
        fprintf(stderr, "No action for varName '%s' with type '%c'.", varName, varType);PRINT_ERROR
        break;
    }
    return 0;
}

void closeEnvFile(char varName[], char fileName[], FILE *file) {
    if (file == NULL || fileName == NULL || ferror(file)) {
        return;
    }
    switch (isStdFile(fileName)) {
    case IS_STDOUT:
        break;
    case IS_STDERR:
        break;
    
    default:
        fclose(file);
        break;
    }
}

void initEnv(int argc, char *argv[], char **envp, mfog_params_t *params) {
    // initMPI(argc, argv, envp, params);
    int mpiReturn;
    mpiReturn = MPI_Init(&argc, &argv);
    if (mpiReturn != MPI_SUCCESS) {
        MPI_Abort(MPI_COMM_WORLD, mpiReturn);
        errx(EXIT_FAILURE, "MPI Abort %d\n", mpiReturn);
    }
    int mpiRank, mpiSize;
    mpiReturn = MPI_Comm_size(MPI_COMM_WORLD, &mpiSize);
    mpiReturn = MPI_Comm_rank(MPI_COMM_WORLD, &mpiRank);
    if (mpiReturn != MPI_SUCCESS) {
        MPI_Abort(MPI_COMM_WORLD, mpiReturn);
        errx(EXIT_FAILURE, "MPI Abort %d\n", mpiReturn);
    }
    printf("MPI rank / size => %d/%d\n", mpiRank, mpiSize);
    params->mpiRank = mpiRank;
    params->mpiSize = mpiSize;
    //
    params->executable = argv[0];
    //
    params->kParam = 100;
    params->dimension = 22;
    params->noveltyThreshold = 2;
    params->minExCluster = 20;
    params->maxUnkSize = params->kParam * params->minExCluster;
    params->thresholdForgettingPast = 10000;
    //
    params->isModelServer = 0;
    params->useModelStore = 0;
    params->trainingCsv = NULL;
    params->trainingFile = NULL;
    params->modelCsv = NULL;
    params->modelFile = NULL;
    params->examplesCsv = NULL;
    params->examplesFile = NULL;
    params->matchesCsv = NULL;
    params->matchesFile = NULL;
    params->timingLog = NULL;
    params->timingFile = NULL;

    int envErrors = 0;
    params->isModelServer += findEnvFlag(argc, argv, envp, "--cloud");

    params->kParamStr = findEnvVar(argc, argv, envp, "k");
    if (params->kParamStr == NULL) {
        envErrors++;
    } else {
        params->kParam = atoi(params->kParamStr);
    }
    
    params->dimensionStr = findEnvVar(argc, argv, envp, "dimension");
    if (params->dimensionStr == NULL) {
        envErrors++;
    } else {
        params->dimension = atoi(params->dimensionStr);
    }
    if (params->mpiRank != 0) {
        return;
    }
    //
    loadEnvFile(argc, argv, envp, TRAINING_CSV,   &params->trainingCsv,   &params->trainingFile,    "r");
    // envErrors += params->trainingFile == NULL;
    loadEnvFile(argc, argv, envp, MODEL_CSV,      &params->modelCsv,      &params->modelFile,       "r");
    params->useModelStore = params->modelFile == NULL;
    // envErrors += params->modelFile == NULL;
    loadEnvFile(argc, argv, envp, EXAMPLES_CSV,   &params->examplesCsv,   &params->examplesFile,    "r");
    if (!params->isModelServer)
        envErrors += params->examplesFile == NULL;
    loadEnvFile(argc, argv, envp, MATCHES_CSV,    &params->matchesCsv,    &params->matchesFile,     "w");
    if (!params->isModelServer)
        envErrors += params->matchesFile == NULL;
    loadEnvFile(argc, argv, envp, TIMING_LOG,     &params->timingLog,     &params->timingFile,      "a");
    // envErrors += params->timingFile == NULL;
    //
    printf(
        "useModelStore          %d\n"
        "isModelServer          %d\n"
        "Using kParam as        %d\n"
        "Using dimension as     %d\n"
        "Reading training from  (%p) '%s'\n"
        "Reading model from     (%p) '%s'\n"
        "Reading examples from  (%p) '%s'\n"
        "Writing matchesFile to (%p) '%s'\n"
        "Writing timingFile to  (%p) '%s'\n",
        params->useModelStore, params->isModelServer, params->kParam, params->dimension,
        params->trainingFile, params->trainingCsv,
        params->modelFile, params->modelCsv,
        params->examplesFile, params->examplesCsv,
        params->matchesFile, params->matchesCsv,
        params->timingFile, params->timingLog);
    fflush(stdout);
    if (envErrors != 0) {
        MPI_Finalize();
        errx(EXIT_FAILURE, "Environment errors %d. At "__FILE__":%d\n", envErrors, __LINE__);
    }
}

#endif // !_LOAD_ENV_C
