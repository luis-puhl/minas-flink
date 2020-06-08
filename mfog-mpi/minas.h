#ifndef MINAS_H
#define MINAS_H
#include <stdio.h>

typedef struct point {
    unsigned int id;
    float *value;
    char label;
} Point;

typedef struct cluster {
    unsigned int id, matches;
    char label, category;
    float radius, meanDistance, *center;
    int time;
} Cluster;

typedef struct model {
    Cluster* vals;
    int size;
} Model;

typedef struct match {
    unsigned int pointId, clusterId;
    char isMatch, label;
    float distance, radius;
} Match;

#ifndef MINAS_FUNCS
#define MINAS_FUNCS
double MNS_distance(float a[], float b[], int dimension);
#endif // MINAS_FUNCS

#endif // MINAS_H