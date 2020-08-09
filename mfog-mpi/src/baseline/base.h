#ifndef _BASE_H
#define _BASE_H

#define assertEquals(val, exp) \
    if (val != exp) errx(EXIT_FAILURE, "Assert error. At "__FILE__":%d\n", __LINE__)

#define assertDiffer(val, exp) \
    if (val == exp) errx(EXIT_FAILURE, "Assert error. At "__FILE__":%d\n", __LINE__)

#define printTiming(setSize) \
    fprintf(stderr, "[%s] %le seconds for %s(%u). At %s:%d\n", \
    params->executable, ((double)(clock() - start)) / 1000000.0, __FUNCTION__, setSize, __FILE__, __LINE__);

typedef struct {
    unsigned int k;
    unsigned int dim;
    unsigned int minExamplesPerCluster;
    double precision;
    double radiusF;
    double noveltyF;
    const char *executable;
    unsigned int useCluStream, cluStream_q_maxMicroClusters;
    double cluStream_time_threshold_delta_δ;
} Params;

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
    double distanceLinearSum, distanceSquareSum;
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
    // int pointId, clusterId;
    // char clusterLabel, clusterCatergoy;
    // double clusterRadius;
    char label;
    double distance; // , secondDistance;
    Cluster *cluster;
    // Example *example;
    char *labelStr;
} Match;

char *printableLabel(char label);

double euclideanSqrDistance(unsigned int dim, double a[], double b[]);
double euclideanDistance(unsigned int dim, double a[], double b[]);

#endif // !_BASE_H
