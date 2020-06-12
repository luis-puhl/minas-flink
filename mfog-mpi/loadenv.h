#ifndef _LOAD_ENV_H
#define _LOAD_ENV_H 1

#include <stdio.h>

int assingVarFromEnvArg(char *varName, char **varPtr, char *envOrArg, char *nextArg);
int loadEnv(int argc, char *argv[], char **envp, int varsSize, char *varNames[], char **varPtrs[], FILE **filePtrs[], char *fileModes[]);

#endif // !_LOAD_ENV_H
