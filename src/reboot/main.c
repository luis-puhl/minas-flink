#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <mpi.h>

#define fail(text, args) \
    errx(EXIT_FAILURE, text". At "__FILE__":%d\n", args, __LINE__)

#define assert(exp) \
    if (!(exp)) errx(EXIT_FAILURE, "Assert error. At "__FILE__":%d\n", __LINE__)

#define assertMsg(exp, text, args) \
    if (!(exp)) errx(EXIT_FAILURE, "Assert error. " text " At "__FILE__":%d\n", args, __LINE__)

typedef struct t_example {
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

char *printableLabel(char label) {
    char *ret = calloc(20, sizeof(char));
    if (isalpha(label) || label == '-') {
        ret[0] = label;
    } else {
        sprintf(ret, "%d", label);
    }
    return ret;
}

double nearestClusterVal(int dim, Cluster clusters[], size_t nClusters, double val[], Cluster **nearest) {
    double minDist;
    *nearest = NULL;
    for (size_t k = 0; k < nClusters; k++) {
        double dist = 0.0;
        for (size_t d = 0; d < dim; d++) {
            double v = (clusters[k].center[d] - val[d]);
            // dist += fabs(v);
            dist += v * v;
            if (k > 0 && dist > minDist) break;
        }
        if (k == 0 || dist <= minDist) {
            minDist = dist;
            *nearest = &clusters[k];
        }
    }
    return sqrt(minDist);
}

Cluster* kMeansInit(int kParam, int dim, Example trainingSet[], unsigned int trainingSetSize, unsigned int initalId) {
    if (trainingSetSize < kParam) {
        errx(EXIT_FAILURE, "Not enough examples for K-means. At "__FILE__":%d\n", __LINE__);
    }
    Cluster *clusters = calloc(kParam, sizeof(Cluster));
    for (size_t i = 0; i < kParam; i++) {
        clusters[i].id = initalId + i;
        clusters[i].n_matches = 0;
        clusters[i].center = calloc(dim, sizeof(double));
        clusters[i].ls_valLinearSum = calloc(dim, sizeof(double));
        clusters[i].ss_valSquareSum = calloc(dim, sizeof(double));
        for (size_t d = 0; d < dim; d++) {
            clusters[i].center[d] = trainingSet[i].val[d];
            clusters[i].ls_valLinearSum[d] = trainingSet[i].val[d];
            clusters[i].ss_valSquareSum[d] = trainingSet[i].val[d] * trainingSet[i].val[d];
        }
    }
    return clusters;
}

double kMeans(int kParam, int dim, double precision, Cluster* clusters, Example trainingSet[], unsigned int trainingSetSize) {
    // clock_t start = clock();
    double improvement, prevGlobalDistance, globalDistance = dim * kParam * trainingSetSize * 2;
    unsigned int iteration = 0;
    do {
        prevGlobalDistance = globalDistance;
        globalDistance = 0.0;
        for (size_t i = 0; i < trainingSetSize; i++) {
            Cluster *nearest = NULL;
            double minDist = nearestClusterVal(dim, clusters, kParam, trainingSet[i].val, &nearest);
            globalDistance += minDist;
            nearest->n_matches++;
            for (size_t d = 0; d < dim; d++) {
                nearest->ls_valLinearSum[d] += trainingSet[i].val[d];
                nearest->ss_valSquareSum[d] += trainingSet[i].val[d] * trainingSet[i].val[d];
            }
        }
        for (size_t k = 0; k < kParam; k++) {
            for (size_t d = 0; d < dim; d++) {
                if (clusters[k].n_matches > 0)
                    clusters[k].center[d] = clusters[k].ls_valLinearSum[d] / clusters[k].n_matches;
                clusters[k].ls_valLinearSum[d] = 0.0;
                clusters[k].ss_valSquareSum[d] = 0.0;
            }
            clusters[k].n_matches = 0;
        }
        improvement = globalDistance - prevGlobalDistance;
        // fprintf(stderr, "\t[%3u] k-Means %le -> %le (%+le)\n", iteration, prevGlobalDistance, globalDistance, improvement);
        iteration++;
    } while (fabs(improvement) > precision && iteration < 100);
    // printTiming(kMeans, trainingSetSize);
    return globalDistance;
}

Cluster* clustering(int kParam, int dim, double precision, double radiusF, Example trainingSet[], unsigned int trainingSetSize, unsigned int initalId) {
    Cluster *clusters = kMeansInit(kParam, dim, trainingSet, trainingSetSize, initalId);
    kMeans(kParam, dim, precision, clusters, trainingSet, trainingSetSize);
    //
    double *distances = calloc(trainingSetSize, sizeof(double));
    Cluster **n_matches = calloc(trainingSetSize, sizeof(Cluster *));
    for (size_t k = 0; k < kParam; k++) {
        clusters[k].radius = 0.0;
        clusters[k].n_matches = 0;
        clusters[k].distanceMax = 0.0;
        clusters[k].distanceLinearSum = 0.0;
        clusters[k].distanceSquareSum = 0.0;
    }
    for (size_t i = 0; i < trainingSetSize; i++) {
        Cluster *nearest = NULL;
        double minDist = nearestClusterVal(dim, clusters, kParam, trainingSet[i].val, &nearest);
        distances[i] = minDist;
        n_matches[i] = nearest;
        //
        nearest->n_matches++;
        nearest->distanceLinearSum += minDist;
        nearest->distanceSquareSum += minDist * minDist;
        if (minDist > nearest->distanceMax) {
            nearest->distanceMax = minDist;
        }
        for (size_t d = 0; d < dim; d++) {
            nearest->ls_valLinearSum[d] += trainingSet[i].val[d];
            nearest->ss_valSquareSum[d] += trainingSet[i].val[d] * trainingSet[i].val[d];
        }
    }
    for (size_t k = 0; k < kParam; k++) {
        if (clusters[k].n_matches == 0) continue;
        clusters[k].distanceAvg = clusters[k].distanceLinearSum / clusters[k].n_matches;
        clusters[k].distanceStdDev = 0.0;
        for (size_t i = 0; i < trainingSetSize; i++) {
            if (n_matches[i] == &clusters[k]) {
                double p = distances[i] - clusters[k].distanceAvg;
                clusters[k].distanceStdDev += p * p;
            }
        }
        clusters[k].distanceStdDev = sqrt(clusters[k].distanceStdDev);
        clusters[k].radius = radiusF * clusters[k].distanceStdDev;
    }
    return clusters;
}

Model *training(int kParam, int dim, double precision, double radiusF) {
    // read training stream
    unsigned int id = 0, nClasses = 0;
    Example *trainingSetByClass[255];
    char classes[255];
    unsigned int classesSize[255];
    for (size_t l = 0; l < 255; l++) {
        trainingSetByClass[l] = calloc(1, sizeof(Example));
        classesSize[l] = 0;
        classes[l] = '\0';
    }
    fprintf(stderr, "Taking training set from stdin\n");
    while (!feof(stdin)) {
        double *value = calloc(dim, sizeof(double));
        for (size_t d = 0; d < dim; d++) {
            scanf("%lf,", &value[d]);
        }
        char class;
        scanf("%c", &class);
        //
        size_t l;
        for (l = 0; classes[l] != '\0'; l++)
            if (classes[l] == class)
                break;
        if (classes[l] == '\0') {
            nClasses++;
            classes[l] = class;
        }
        assert(nClasses != 254);
        classesSize[l]++;
        trainingSetByClass[l] = realloc(trainingSetByClass[l], classesSize[l] * sizeof(Example));
        Example *ex = &trainingSetByClass[l][classesSize[l] -1];
        //
        ex->id = id;
        ex->val = value;
        ex->label = class;
        id++;
        // if (id > 71990) fprintf(stderr, "Ex(id=%d, val=%le, class=%c)\n", ex->id, ex->val[0], ex->class);
        //
        int hasEmptyline;
        scanf("\n%n", &hasEmptyline);
        if (hasEmptyline == 2) break;
    }
    //
    fprintf(stderr, "Training %u examples with %d classes (%s)\n", id, nClasses, classes);
    if (id < kParam) {
        errx(EXIT_FAILURE, "Not enough examples for training. At "__FILE__":%d\n", __LINE__);
    }
    Model *model = calloc(1, sizeof(Model));
    model->size = 0;
    model->nextLabel = '\0';
    model->clusters = calloc(1, sizeof(Cluster));
    for (size_t l = 0; l < nClasses; l++) {
        Example *trainingSet = trainingSetByClass[l];
        unsigned int trainingSetSize = classesSize[l];
        char class = classes[l];
        fprintf(stderr, "Training %u examples from class %c\n", trainingSetSize, class);
        Cluster *clusters = clustering(kParam, dim, precision, radiusF, trainingSet, trainingSetSize, model->size);
        //
        unsigned int prevSize = model->size;
        model->size += kParam;
        model->clusters = realloc(model->clusters, model->size * sizeof(Cluster));
        for (size_t k = 0; k < kParam; k++) {
            clusters[k].label = class;
            model->clusters[prevSize + k] = clusters[k];
        }
        free(clusters);
    }
    return model;
}

Match *identify(int kParam, int dim, double precision, double radiusF, Model *model, Example *example, Match *match) {
    // Match *match = calloc(1, sizeof(Match));
    match->label = UNK_LABEL;
    match->distance = nearestClusterVal(dim, model->clusters, model->size, example->val, &match->cluster);
    assert(match->cluster != NULL);
    if (match->distance <= match->cluster->radius) {
        match->label = match->cluster->label;
    }
    return match;
}

#define PARAMS kParam, dim, precision, radiusF, minExamplesPerCluster, noveltyF
#define PARAMS_ARG int kParam, int dim, double precision, double radiusF, int minExamplesPerCluster, double noveltyF

void noveltyDetection(PARAMS_ARG, Model *model, Example *unknowns, size_t unknownsSize) {
    Cluster *clusters = clustering(kParam, dim, precision, radiusF, unknowns, unknownsSize, model->size);
    int extensions = 0, novelties = 0;
    for (size_t k = 0; k < kParam; k++) {
        if (clusters[k].n_matches < minExamplesPerCluster) continue;
        //
        Cluster *nearest = NULL;
        double minDist = nearestClusterVal(dim, model->clusters, model->size, clusters[k].center, &nearest);
        if (minDist <= noveltyF * nearest->distanceStdDev) {
            clusters[k].label = nearest->label;
            extensions++;
        } else {
            clusters[k].label = model->nextLabel;
            // inc label
            do {
                model->nextLabel = (model->nextLabel + 1) % 255;
            } while (isalpha(model->nextLabel) || model->nextLabel == '-');
            // fprintf(stderr, "Novelty %s\n", printableLabel(clusters[k].label));
            novelties++;
        }
        //
        clusters[k].id = model->size;
        model->size++;
        model->clusters = realloc(model->clusters, model->size * sizeof(Cluster));
        model->clusters[model->size - 1] = clusters[k];
    }
    fprintf(stderr, "ND clusters: %d extensions, %d novelties\n", extensions, novelties);
    free(clusters);
}

void minasOnline(PARAMS_ARG, Model *model) {
    unsigned int id = 0;
    Match match;
    Example example;
    example.val = calloc(dim, sizeof(double));
    printf("#pointId,label\n");
    size_t unknownsMaxSize = minExamplesPerCluster * kParam;
    size_t noveltyDetectionTrigger = minExamplesPerCluster * kParam;
    Example *unknowns = calloc(unknownsMaxSize, sizeof(Example));
    size_t unknownsSize = 0;
    size_t lastNDCheck = 0;
    int hasEmptyline = 0;
    fprintf(stderr, "Taking test stream from stdin\n");
    while (!feof(stdin) && hasEmptyline != 2) {
        for (size_t d = 0; d < dim; d++) {
            assert(scanf("%lf,", &example.val[d]));
        }
        // ignore class
        char class;
        assert(scanf("%c", &class));
        example.id = id;
        id++;
        scanf("\n%n", &hasEmptyline);
        //
        identify(kParam, dim, precision, radiusF, model, &example, &match);
        printf("%10u,%s\n", example.id, printableLabel(match.label));
        //
        if (match.label != UNK_LABEL) continue;
        unknowns[unknownsSize] = example;
        unknowns[unknownsSize].val = calloc(dim, sizeof(double));
        for (size_t d = 0; d < dim; d++) {
            unknowns[unknownsSize].val[d] = example.val[d];
        }
        unknownsSize++;
        if (unknownsSize >= unknownsMaxSize) {
            unknownsMaxSize *= 2;
            unknowns = realloc(unknowns, unknownsMaxSize * sizeof(Example));
        }
        //
        if (unknownsSize % noveltyDetectionTrigger == 0 && id - lastNDCheck > noveltyDetectionTrigger) {
            lastNDCheck = id;
            unsigned int prevSize = model->size;
            noveltyDetection(PARAMS, model, unknowns, unknownsSize);
            unsigned int nNewClusters = model->size - prevSize;
            //
            size_t reclassified = 0;
            for (size_t ex = 0; ex < unknownsSize; ex++) {
                // compress
                unknowns[ex - reclassified] = unknowns[ex];
                Cluster *nearest;
                double distance = nearestClusterVal(dim, &model->clusters[prevSize], nNewClusters, unknowns[ex].val, &nearest);
                assert(nearest != NULL);
                if (distance <= nearest->distanceMax) {
                    reclassified++;
                }
            }
            fprintf(stderr, "Reclassified %lu\n", reclassified);
            unknownsSize -= reclassified;
        }
    }
    fprintf(stderr, "Final flush %lu\n", unknownsSize);
    // final flush
    if (unknownsSize > kParam) {
        unsigned int prevSize = model->size;
        noveltyDetection(PARAMS, model, unknowns, unknownsSize);
        unsigned int nNewClusters = model->size - prevSize;
        //
        size_t reclassified = 0;
        for (size_t ex = 0; ex < unknownsSize; ex++) {
            // compress
            unknowns[ex - reclassified] = unknowns[ex];
            Cluster *nearest;
            double distance = nearestClusterVal(dim, &model->clusters[prevSize], nNewClusters, unknowns[ex].val, &nearest);
            assert(nearest != NULL);
            if (distance <= nearest->distanceMax) {
                printf("%10u,%s\n", unknowns[ex].id, printableLabel(nearest->label));
                reclassified++;
            }
        }
        fprintf(stderr, "Reclassified %lu\n", reclassified);
        unknownsSize -= reclassified;
    }
}

int main(int argc, char const *argv[]) {
    int kParam = 100, dim = 22, minExamplesPerCluster = 20;
    double precision = 1.0e-08, radiusF = 0.10, noveltyF = 2.0;
    //
    kParam=100; dim=22; precision=1.0e-08; radiusF=0.25; minExamplesPerCluster=20; noveltyF=1.4;
    // kParam=100; dim=22; precision=1.0e-08; radiusF=0.10; minExamplesPerCluster=20; noveltyF=2.0;
    //
    fprintf(stderr, "kParam=%d; dim=%d; precision=%le; radiusF=%le; minExamplesPerCluster=%d; noveltyF=%le\n", PARAMS);
    Model *model = training(kParam, dim, precision, radiusF);
    minasOnline(PARAMS, model);
    free(model);
    return EXIT_SUCCESS;
}
