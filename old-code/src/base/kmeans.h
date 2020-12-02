#ifndef _KMEANS_H
#define _KMEANS_H

#include "./base.h"
#include "./minas.h"

Cluster* kMeansInit(Params *params, Example trainingSet[], unsigned int trainingSetSize, unsigned int initalId);
double kMeans(Params *params, Cluster* clusters, Example trainingSet[], unsigned int trainingSetSize);

#endif // !_KMEANS_H
