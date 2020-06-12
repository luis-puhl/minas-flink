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
} Match;

// #ifndef MINAS_FUNCS
// #define MINAS_FUNCS
// double MNS_distance(double a[], double b[], int dimension);
// #endif // MINAS_FUNCS
double MNS_distance(double a[], double b[], int dimension);
void readModel(int dimension, FILE *file, Model *model, FILE *timing, char *executable);
Point *readExamples(int dimension, FILE *file, FILE *timing, char *executable);
void classify(int dimension, Model *model, Point *ex, Match *match);

#endif // MINAS_H