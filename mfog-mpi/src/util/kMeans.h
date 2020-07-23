#include "../minas/minas.h"
#include "../util/loadenv.h"

Cluster *kMeansInit(int k, Cluster clusters[], int dimension, Point *examples[], int initialClusterId, char label, char category, FILE *timing, char *executable);
Cluster *kMeans(int k, Cluster clusters[], int dimension, Point *examples[], int nExamples, FILE *timing, char *executable);
