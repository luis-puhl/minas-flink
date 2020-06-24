#ifndef _LOAD_ENV_H
#define _LOAD_ENV_H 1

#include <stdio.h>

int assingVarFromEnvArg(char *varName, char **varPtr, char *envOrArg, char *nextArg);
int loadEnv(int argc, char *argv[], char **envp, int varsSize, char *varNames[], char **varPtrs[], FILE **filePtrs[], char *fileModes[]);
void closeEnv(int varsSize, char *varNames[], char **varPtrs[], FILE **filePtrs[], char *fileModes[]);

// # source, executable, build_date-time, wall-clock, function, elapsed, cores, itens
#define PRINT_TIMING_FORMAT "%s,%s,%s %s,%ld,%s,%e,%d,%d\n"
#define PRINT_TIMING_ARGUMENTS(executable, nProcesses, start, nItens) \
    __FILE__, executable, __DATE__, __TIME__, time(NULL), __FUNCTION__, ((double)(clock() - start)) / 1000000.0, nProcesses, nItens
#define PRINT_TIMING(timing, executable, nProcesses, start, nItens)                                      \
    fprintf(timing, PRINT_TIMING_FORMAT, PRINT_TIMING_ARGUMENTS(executable, nProcesses, start, nItens)); \
    fprintf(stderr, PRINT_TIMING_FORMAT, PRINT_TIMING_ARGUMENTS(executable, nProcesses, start, nItens))

#endif // !_LOAD_ENV_H
