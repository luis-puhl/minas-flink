#include "../minas/minas.h"
#include "../util/loadenv.h"

Model *kMeansInit(int nClusters, int dimension, Point examples[]);
Model *kMeans(Model *model, int nClusters, int dimension, Point examples[], int nExamples, FILE *timing, char *executable);
