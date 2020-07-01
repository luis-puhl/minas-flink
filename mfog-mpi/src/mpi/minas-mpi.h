#ifndef MFOG_H
#define MFOG_H

#include "../minas/minas.h"

void sendModel(int dimension, Model *model, int clRank, int clSize, FILE *timing, char *executable);
void receiveModel(int dimension, Model *model, int clRank);
int receiveClassifications(FILE *matches);
int sendExamples(int dimension, Point *examples, int clSize, FILE *matches, FILE *timing, char *executable);
int receiveExamples(int dimension, Model *model, int clRank);
int MNS_mfog_main(int argc, char *argv[], char **envp);

#endif // MFOG_H
