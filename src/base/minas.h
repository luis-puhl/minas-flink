#ifndef _MINAS_H
#define _MINAS_H 1

#include <stdio.h>

#include "./base.h"

typedef struct {
    unsigned int id;
    char label;
    double *val;
} Example;

typedef struct {
    unsigned int id, n_matches;
    char label;
    double *center;
    double *ls_valLinearSum, *ss_valSquareSum;
    // double *valAverage, *valStdDev;
    double distanceLinearSum, distanceSquareSum, distanceMax;
    double timeLinearSum, timeSquareSum;
    double distanceAvg, distanceStdDev, radius;
    double time_mu_μ, time_sigma_σ;
    // assumed last m arrivals in each micro-cluster to be __m = n__
    // so, the m/(2 · n)-th percentile is the 50th percentile
    // therefore z-indez is 0.0 and time_relevance_stamp is the mean distance;
    // double time_relevance_stamp_50;
    // unsigned int *ids, idSize; // used only to reconstruct clusters from snapshot
} Cluster;

typedef struct {
    Cluster *clusters;
    unsigned int size;
    unsigned int nextLabel;
} Model;

typedef struct {
    int pointId, clusterId;
    // char clusterLabel, clusterCatergoy;
    // double clusterRadius;
    char label;
    double distance; // , secondDistance;
    Cluster *cluster;
    // Example *example;
    char *labelStr;
} Match;

#define UNK_LABEL '-'

char *printableLabel(char label);
double nearestClusterVal(Params *params, Cluster clusters[], size_t nClusters, double val[], Cluster **nearest);
Example *next(Params *params, Example **reuse);

Cluster* clustering(Params *params, Example trainingSet[], unsigned int trainingSetSize, unsigned int initalId);
Model *training(Params *params);
Match *identify(Params *params, Model *model, Example *example, Match *match);
void noveltyDetection(Params *params, Model *model, Example *unknowns, size_t unknownsSize);
void minasOnline(Params *params, Model *model);

#endif // !_MINAS_H