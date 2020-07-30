#ifndef _LOAD_ENV_H
#define _LOAD_ENV_H 1

#include <stdio.h>

char* findEnvVar(int argc, char *argv[], char **envp, char *varName);
int loadEnvVar(int argc, char *argv[], char **envp, char varType, char *varName, char **strVarPtr, int *valuePtr);
FILE *loadEnvFile(int argc, char *argv[], char **envp, char *varName, char **strVarPtr, FILE **valuePtr, char fileMode[]);
void closeEnvFile(char varName[], char fileName[], FILE *file);

int findEnvFlag(int argc, char *argv[], char **envp, char *varName);

// # source, executable, build_date-time, wall-clock, function, gnuc, elapsed, cores, itens
#define PRINT_TIMING_FORMAT "%s,%s,%s %s,%ld,%s,%s,%e,%d,%d\n"
#define PRINT_TIMING_ARGUMENTS(executable, nProcesses, start, nItens) \
    __FILE__, executable, __DATE__, __TIME__, time(NULL), __FUNCTION__, __VERSION__, ((double)(clock() - start)) / 1000000.0, nProcesses, nItens
#define PRINT_TIMING(timing, executable, nProcesses, start, nItens)                                      \
    fprintf(timing, PRINT_TIMING_FORMAT, PRINT_TIMING_ARGUMENTS(executable, nProcesses, start, nItens)); \
    fprintf(stderr, PRINT_TIMING_FORMAT, PRINT_TIMING_ARGUMENTS(executable, nProcesses, start, nItens))

#endif // !_LOAD_ENV_H
