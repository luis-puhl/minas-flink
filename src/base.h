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
    unsigned int id, n_matches, n_misses, latest_match_id, extensionOF, evictions;
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


typedef struct ModelLink_st {
    Cluster cluster;
    struct ModelLink_st *next;
    unsigned int rank;
} ModelLink;

typedef struct {
    ModelLink *head, *tail;
    unsigned int size, nextLabel, nextId;
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

typedef struct MinasParams_st {
    unsigned int k;
    unsigned int dim;
    double precision;
    double radiusF;
    unsigned int minExamplesPerCluster;
    double noveltyF;
    unsigned int thresholdForgettingPast;
    unsigned long noveltyDetectionTrigger;
    unsigned long unknownsMaxSize;
} MinasParams;

typedef struct MinasState_st {
    Model model, sleep;
    Example *unknowns;
    unsigned long unknownsSize;
    unsigned long lastNDCheck;
    unsigned long lastForgetCheck;
    unsigned long currId;
    unsigned int noveltyCount;
} MinasState;

#define MINAS_UNK_LABEL '-'

#define MINAS_STATE_EMPTY { \
        .noveltyCount = 0, .unknownsSize = 0, .lastNDCheck = 0, .currId = 0, \
        .model = { .size = 0, .nextLabel = 0, .nextId = 0, .head = NULL, .tail = NULL }, \
        .sleep = { .size = 0, .nextLabel = 0, .nextId = 0, .head = NULL, .tail = NULL }, \
    };

#define printArgs(minasParams, outputMode, nClassifiers)                                                           \
    fprintf(stderr, "%s; kParam=%d; dim=%d; precision=%le; radiusF=%le; minExamplesPerCluster=%d; noveltyF=%le; outputMode %d, nClassifiers %d\n", \
            argv[0], minasParams.k, minasParams.dim, minasParams.precision, minasParams.radiusF,                   \
            minasParams.minExamplesPerCluster, minasParams.noveltyF, outputMode, nClassifiers);

char *printableLabel(unsigned int label);
char *printableLabelReuse(unsigned int label, char *ret);
unsigned int fromPrintableLabel(char *label);
double nearestClusterVal(int dim, ModelLink *head, unsigned int limit, double val[], Cluster **nearest);
ModelLink *kMeansInit(int kParam, int dim, Example trainingSet[], unsigned int trainingSetSize);
double kMeans(int kParam, int dim, double precision, ModelLink *head, Example trainingSet[], unsigned int trainingSetSize);
//

ModelLink *clustering(MinasParams *params, Example trainingSet[], unsigned int trainingSetSize);
Model *training(MinasParams *params);

Match *identify(MinasParams *params, Model *model, Example *example, Match *match);
void minasHandleSleep(MinasParams *params, MinasState *state);

unsigned int noveltyDetection(MinasParams *params, MinasState *state, unsigned int *noveltyCount);
unsigned int minasHandleUnknown(MinasParams *params, MinasState *state, Example *example);
//

int readCluster(int kParam, int dim, Cluster *cluster, char lineptr[]);
void restoreSleep(MinasParams *params, MinasState *state);
Cluster *addCluster(int dim, Cluster *cluster, Model *model);

char getMfogLine(FILE *fd, char **line, size_t *lineLen, unsigned int kParam, unsigned int dim, Cluster *cluster, Example *example);

int printCluster(int dim, Cluster *cl, char *reuseLabel);
char *labelMatchStatistics(Model *model, char *stats);

#endif // _BASE_H
