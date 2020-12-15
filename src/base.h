#ifndef _BASE_H
#define _BASE_H 1

#include <stdio.h>
#include <err.h>
#include <string.h>

#define fail(text, args) \
    errx(EXIT_FAILURE, text". At "__FILE__":%d\n", args, __LINE__)

#define assert(exp) \
    if (!(exp)) errx(EXIT_FAILURE, "Assert error. At "__FILE__":%d\n", __LINE__)

#define assertNotNull(exp) \
    if ((exp) == NULL) errx(EXIT_FAILURE, "Assert NULL error. At "__FILE__":%d\n", __LINE__)

#define assertMsg(exp, text, arg) \
    if (!(exp)) errx(EXIT_FAILURE, "Assert error. " text " At "__FILE__":%d\n", arg, __LINE__)
#define assertErrno(exp, text, args, extra) \
    if (!(exp)) { \
        char err_msg[256]; \
        strerror_r(errno, err_msg, 256); \
        extra; \
        errx(EXIT_FAILURE, "Error: %s." text " At "__FILE__":%d\n", err_msg, args, __LINE__); \
    }
#define marker(txt) fprintf(stderr, txt" "__FILE__":%d\n", __LINE__);

#define PARAMS kParam, dim, precision, radiusF, minExamplesPerCluster, noveltyF, thresholdForgettingPast
#define PARAMS_ARG unsigned int kParam, unsigned int dim, double precision, double radiusF, unsigned int thresholdForgettingPast, unsigned int minExamplesPerCluster, double noveltyF

typedef struct t_example {
    unsigned int id;
    unsigned int label;
    double *val;
} Example;

typedef struct {
    unsigned int id, n_matches, n_misses, latest_match_id, extensionOF;
    unsigned int label, isIntrest;
    double *center;
    double *ls_valLinearSum, *ss_valSquareSum;
    // double *valAverage, *valStdDev;
    double distanceLinearSum, distanceSquareSum, distanceMax;
    double timeLinearSum, timeSquareSum;
    double distanceAvg, distanceStdDev, radius;
    // double time_mu_μ, time_sigma_σ;
    // assumed last m arrivals in each micro-cluster to be __m = n__
    // so, the m/(2 · n)-th percentile is the 50th percentile
    // therefore z-index is 0.0 and time_relevance_stamp is the mean distance;
    // double time_relevance_stamp_50;
    // unsigned int *ids, idSize; // used only to reconstruct clusters from snapshot
} Cluster;

typedef struct {
    Cluster *clusters;
    Cluster *activeClusters;
    unsigned int size, activeSize, nextLabel;
} Model;

typedef struct {
    unsigned int pointId, clusterId;
    // char clusterLabel, clusterCatergoy;
    // double clusterRadius;
    unsigned int label, isMatch;
    double distance; // , secondDistance;
    Cluster *cluster;
    // Example *example;
    char *labelStr;
} Match;

#define MINAS_UNK_LABEL '-'

char *printableLabel(unsigned int label);
char *printableLabelReuse(unsigned int label, char *ret);
unsigned int fromPrintableLabel(char *label);
double nearestClusterVal(int dim, Cluster clusters[], size_t nClusters, double val[], Cluster **nearest);
Cluster* kMeansInit(int kParam, int dim, Example trainingSet[], unsigned int trainingSetSize, unsigned int initalId);
double kMeans(int kParam, int dim, double precision, Cluster* clusters, Example trainingSet[], unsigned int trainingSetSize);
Cluster* clustering(int kParam, int dim, double precision, double radiusF, Example trainingSet[], unsigned int trainingSetSize, unsigned int initalId);
Model *training(int kParam, int dim, double precision, double radiusF);
Match *identify(int kParam, int dim, double precision, double radiusF, Model *model, Example *example, Match *match, unsigned int thresholdForgettingPast);
unsigned int noveltyDetection(PARAMS_ARG, Model *model, Example *unknowns, size_t unknownsSize, unsigned int *noveltyCount);

int readCluster(int kParam, int dim, Cluster *cluster, char lineptr[]);
Cluster *addCluster(int kParam, int dim, Cluster *cluster, Model *model);

char getMfogLine(FILE *fd, char **line, unsigned long *lineLen, int kParam, int dim, unsigned long *id, Model *model, Cluster *cluster, Example *example);

int printCluster(int dim, Cluster *cl);
char *labelMatchStatistics(Model *model, char *stats);

#endif // _BASE_H
