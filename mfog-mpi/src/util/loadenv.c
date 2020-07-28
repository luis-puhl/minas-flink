#ifndef _LOAD_ENV_C
#define _LOAD_ENV_C 1

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <err.h>
// #include <string.h>

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
    fprintf(stderr, "Parameter '%s' not found in args or evn.", varName);PRINT_ERROR
    return NULL;
}

#define IS_STDOUT 'o'
#define IS_STDERR 'e'
char isStdFile(char fileName[]) {
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
            fprintf(stderr, "Using default value for param '%s' with value '%s' (%p).", varName, *fileName, *file);PRINT_ERROR
        } else if (*fileName != NULL) {
            *(file) = loadFileFromEnvValue(varName, *fileName, fileMode);
            fprintf(stderr, "Using default string for param '%s' with value '%s' (%p).", varName, *fileName, *file);PRINT_ERROR
        } else if (fileMode[0] == 'w' || fileMode[0] == 'a') {
            fprintf(stderr, "Expected argument or environment '%s' to be defined and '%s'-able."
                "\tWill use stdout as append file.", varName, fileMode);
            PRINT_ERROR
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
    if (fileName == NULL) {
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

#endif // !_LOAD_ENV_C
