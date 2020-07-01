#ifndef MINAS_H
#define MINAS_H
#include <stdio.h>

typedef struct point {
    int id;
    double *value;
    char label;
} Point;

typedef struct cluster {
    int id, matches;
    char label, category;
    double radius, meanDistance, *center;
    int time;
} Cluster;

typedef struct model {
    Cluster* vals;
    int size, dimension;
} Model;

typedef struct match {
    int pointId, clusterId;
    char isMatch, label;
    double distance, radius;
    // Cluster *cluster;
} Match;

// #ifndef MINAS_FUNCS
// #define MINAS_FUNCS
// double MNS_distance(double a[], double b[], int dimension);
// #endif // MINAS_FUNCS
double MNS_distance(double a[], double b[], int dimension);
void readModel(int dimension, FILE *file, Model *model, FILE *timing, char *executable);
Point *readExamples(int dimension, FILE *file, int *nExamples, FILE *timing, char *executable);
void classify(int dimension, Model *model, Point *ex, Match *match);

Model *kMeansInit(int nClusters, int dimension, Point examples[]);
Model *kMeans(Model *model, int nClusters, int dimension, Point examples[], int nExamples, FILE *timing, char *executable);

#endif // MINAS_H