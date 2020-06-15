#ifndef MINAS_H
#define MINAS_H
#include <stdio.h>

typedef struct point {
    unsigned int id;
    double *value;
    char label;
} Point;

typedef struct cluster {
    unsigned int id, matches;
    char label, category;
    double radius, meanDistance, *center;
    int time;
} Cluster;

typedef struct model {
    Cluster* vals;
    int size;
} Model;

typedef struct match {
    unsigned int pointId, clusterId;
    char isMatch, label;
    double distance, radius;
} Match;

// #ifndef MINAS_FUNCS
// #define MINAS_FUNCS
// double MNS_distance(double a[], double b[], int dimension);
// #endif // MINAS_FUNCS
double MNS_distance(double a[], double b[], int dimension);
void readModel(int dimension, char *modelName, Model *model);
Point *readExamples(int dimension, char *testName);
void classify(int dimension, Model *model, Point *ex, Match *match);

#endif // MINAS_H