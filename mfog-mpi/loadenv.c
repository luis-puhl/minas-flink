#ifndef _LOAD_ENV_C
#define _LOAD_ENV_C 1

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <err.h>

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
    for (char **env = envp; *env != 0; env++) {
        char *thisEnv = *env;
        int diff = 0, i = 0;
        for (; prefixMfog[i] != '\0' && diff == 0; i++) diff += prefixMfog[i] - thisEnv[i];
        //
        if (diff != 0) continue;
        for (int var = 0; var < varsSize; var++) {
            if (assingVarFromEnvArg(varNames[var], varPtrs[var], &(thisEnv[i]), NULL)) break;
        }
    }
    for (int arg = 1; arg < argc; arg++) {
        for (int var = 0; var < varsSize; var++) {
            if (assingVarFromEnvArg(varNames[var], varPtrs[var], argv[arg], argv[arg+1])) break;
        }
    }
    int failures = 0;
    for (int var = 0; var < varsSize; var++) {
        if (*(varPtrs[var]) == NULL) {
            printf("Expected argument or environment '%s' to be defined\n", varNames[var]);
            failures++;
            continue;
        }
        *(filePtrs[var]) = fopen(*(varPtrs[var]), fileModes[var]);
        if (*(filePtrs[var]) == NULL) {
            printf("Expected argument '%s' set to '%s' to be '%s'-able file.\n", varNames[var], *(varPtrs[var]), fileModes[var]);
            failures++;
        }
    }
    if (failures > 0) {
        errx(EXIT_FAILURE, "Missing %d arguments.\n", failures);
    }
    return varsSize;
}

#endif // !_LOAD_ENV_C
