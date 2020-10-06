#ifndef _BASE_H
#define _BASE_H 1

#include <stdlib.h>
#include <err.h>

#define assertEquals(val, exp) \
    if (val != exp) errx(EXIT_FAILURE, "Assert error. At "__FILE__":%d\n", __LINE__)

#define assertDiffer(val, exp) \
    if (val == exp) errx(EXIT_FAILURE, "Assert error. At "__FILE__":%d\n", __LINE__)

typedef struct {
    size_t functionPtr, setSize, calls;
    int functionLine;
    char *functionName, *functionFile;
    clock_t time;
} TimingLogEntry;

typedef struct {
    TimingLogEntry *listHead;
    int maxLen, len;
} TimingLog;

typedef struct {
    unsigned int k, dim, minExamplesPerCluster;
    double precision, radiusF, noveltyF;
    const char *executable;
    unsigned int useCluStream, cluStream_q_maxMicroClusters;
    double cluStream_time_threshold_delta_δ;
    int mpiRank, mpiSize;
    TimingLog *log;
    unsigned int useRedis, useMPI, useInlineND, useReclassification;
    char *remoteRedis;
} Params;

// getParam("remoteRedis", "%s", &(params.remoteRedis))

double euclideanSqrDistance(unsigned int dim, double a[], double b[]);
double euclideanDistance(unsigned int dim, double a[], double b[]);

int addTimeLog(Params *params, size_t functionPtr, clock_t time, size_t setSize, char *functionName, char *functionFile, int functionLine);
int printTimeLog(Params *params);

Params* setup(int argc, char const *argv[], char *env[]);
void tearDown(int argc, char const *argv[], char *env[], Params *params);


#define printTiming(funcPtr, setSize) \
    addTimeLog(params, (size_t)funcPtr, clock() - start, setSize, (char *)__FUNCTION__, (char *)__FILE__, __LINE__);

#endif // !_BASE_H
