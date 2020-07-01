
#include "../minas/minas.h"

void sendModel(int dimension, Model *model, int clRank, int clSize, FILE *timing, char *executable);
void receiveModel(int dimension, Model *model, int clRank);
int receiveClassifications(FILE *matches);
int sendExamples(int dimension, Point *examples, int clSize, FILE *matches, FILE *timing, char *executable);
int receiveExamples(int dimension, Model *model, int clRank);
