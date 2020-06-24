#ifndef _LOAD_ENV_C
#define _LOAD_ENV_C 1

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <err.h>
// #include <string.h>

int assingVarFromEnvArg(char *varName, char **varPtr, char *envOrArg, char *nextArg) {
    // printf("varName=%s, envOrArg=%s, nextArg=%s\n", varName, envOrArg, nextArg);
    int diff = 0, i;
    for (i = 0; varName[i] != '\0' && diff == 0; i++) diff += varName[i] - envOrArg[i];
    if (diff != 0) return 0;
    if (envOrArg[i] == '=') {
        *varPtr = &(envOrArg[i + 1]);
    } else if (envOrArg[i] == '\0') {
        *varPtr = nextArg;
    } else {
        return 0;
    }
    // printf("Assing '%s' with '%s'\n", varName, *varPtr);
    return 1;
}

int loadEnv(int argc, char *argv[], char **envp, int varsSize, char *varNames[], char **varPtrs[], FILE **filePtrs[], char *fileModes[]) {
    const char *prefixMfog = "MFOG_";
    int assingned = 0;
    for (char **env = envp; *env != 0; env++) {
        char *thisEnv = *env;
        // fprintf(stderr, "%s\n", thisEnv);
        int diff = 0, i = 0;
        for (; prefixMfog[i] != '\0' && diff == 0; i++) diff += prefixMfog[i] - thisEnv[i];
        //
        if (diff != 0) continue;
        for (int var = 0; var < varsSize; var++) {
            if (assingVarFromEnvArg(varNames[var], varPtrs[var], &(thisEnv[i]), NULL)) {
                assingned++;
                break;
            }
        }
    }
    for (int arg = 1; arg < argc; arg++) {
        for (int var = 0; var < varsSize; var++) {
            // fprintf(stderr, "%s\n", argv[arg]);
            if (assingVarFromEnvArg(varNames[var], varPtrs[var], argv[arg], argv[arg+1])) {
                assingned++;
                break;
            }
        }
    }
    //
    const char *stdoutName = "stdout";
    const char *stderrName = "stderr";
    int failures = 0;
    #define DEBUG_LN fprintf(stderr, "%d %s\n", __LINE__, __FUNCTION__); fflush(stderr);
    for (int var = 0; var < varsSize; var++) {
        if (var >= assingned) {
            printf("Expected argument or environment '%s' to be defined\n", varNames[var]);
            failures++;
            continue;
        }
        if (varPtrs[var] == NULL) break;
        char *fileName = *(varPtrs[var]);
        // fprintf(stderr, "%s => %s\n", varNames[var], fileName);
        if (fileName == NULL) {
            printf("Expected argument or environment '%s' to be defined\n", varNames[var]);
            failures++;
            continue;
        }
        int isStdout = 0, isStderr = 0, i;
        for (i = 0; stdoutName[i] != '\0'; i++) {
            isStdout += stdoutName[i] - fileName[i];
            isStderr += stderrName[i] - fileName[i];
            if (fileName[i] == '\0') break;
        }
        if (isStdout == 0) {
        // if (strcmp("stdout", fileName) == 0) {
            // printf("Set var '%s' to stdout.\n", varNames[var]);
            *(filePtrs[var]) = stdout;
        } else if (isStderr == 0) {
        // } else if (strcmp("stderr", fileName) == 0) {
            // printf("Set var '%s' to stderr.\n", varNames[var]);
            *(filePtrs[var]) = stderr;
        } else {
            *(filePtrs[var]) = fopen(fileName, fileModes[var]);
        }
        if (*(filePtrs[var]) == NULL) {
            printf("Expected argument '%s' set to '%s' to be '%s'-able file.\n", varNames[var], fileName, fileModes[var]);
            failures++;
        }
    }
    if (failures > 0) {
        errx(EXIT_FAILURE, "Missing %d arguments.", failures);
    }
    return varsSize;
}

void closeEnv(int varsSize, char *varNames[], char **varPtrs[], FILE **filePtrs[], char *fileModes[]) {
    const char *stdoutName = "stdout";
    const char *stderrName = "stderr";
    for (int var = 0; var < varsSize; var++) {
        char *fileName = *(varPtrs[var]);
        int isStdout = 0;
        for (int i = 0; stdoutName[i] != '\0' && isStdout == 0; i++) isStdout += stdoutName[i] - fileName[i];
        int isStderr = 0;
        for (int i = 0; stderrName[i] != '\0' && isStderr == 0; i++) isStderr += stderrName[i] - fileName[i];
        if (isStdout || isStderr) {
            continue;
        } else {
            fclose(*(filePtrs[var]));
        }
    }
}

#endif // !_LOAD_ENV_C
