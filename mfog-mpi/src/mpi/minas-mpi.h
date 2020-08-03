#ifndef _MINAS_MPI_H
#define _MINAS_MPI_H 1

#include "../minas/minas.h"

void sendModel(Model *model, int clRank, int clSize, FILE *timing, char *executable);
void receiveModel(Model *model, int clRank);

int receiveClassifications(Match *matches);
int sendExamples(int dimension, Point *examples, Match *matches, int clSize, FILE *timing, char *executable);
int receiveExamples(int dimension, Model *model, int clRank);

int MNS_mfog_main(int argc, char *argv[], char **envp);

// #include "../util/loadenv.h"
// void initMPI(int argc, char *argv[], char **envp, mfog_params_t *params);

#endif // _MINAS_MPI_H
