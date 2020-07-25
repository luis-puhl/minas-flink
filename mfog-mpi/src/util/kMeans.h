#include "../minas/minas.h"
#include "../util/loadenv.h"

Cluster *kMeansInit(int dimension, int k, Cluster clusters[], int nExamples,
    Point examples[], int initialClusterId, char label, char category,
    FILE *timing, char *executable);
Cluster *kMeans(int dimension, int k, Cluster clusters[], int nExamples,
    Point examples[], FILE *timing, char *executable);
