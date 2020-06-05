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

int MNS_dimesion;

double MNS_distance(float a[], float b[]);
int MNS_readExample(Point *ex, FILE *file);
int MNS_printFloatArr(float* value);
int MNS_printPoint(Point *point);
int MNS_printCluster(Cluster *cl);
int MNS_printModel(Model* model);
int MNS_readCluster(char *line, Cluster *cl, const char *filename, const char *sep);
Model *MNS_readModelFile(const char *filename);
