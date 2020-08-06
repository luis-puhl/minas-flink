#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <math.h>

#define assertEquals(val, exp) \
    if (val != exp) errx(EXIT_FAILURE, "Assert error. At "__FILE__":%d\n", __LINE__)

#define assertDiffer(val, exp) \
    if (val == exp) errx(EXIT_FAILURE, "Assert error. At "__FILE__":%d\n", __LINE__)

typedef struct {
    unsigned int k;
    unsigned int dim;
    unsigned int minExamplesPerCluster;
    double precision;
    double radiusF;
} Params;

typedef struct {
    unsigned int id;
    char class;
    double *val;
} Example;

typedef struct {
    unsigned int id, matches;
    char label;
    double *center;
    double *linearSum;
    double *squareSum;
    double distanceLinearSum;
    double distanceSquareSum;
    double radius;
} Cluster;

typedef struct {
    Cluster *clusters;
    unsigned int size;
} Model;

typedef struct {
    // int pointId, clusterId;
    // char clusterLabel, clusterCatergoy;
    // double clusterRadius;
    char label;
    double distance; // , secondDistance;
    Cluster *cluster;
    // Example *example;
} Match;

double euclideanDistance(unsigned int dim, double a[], double b[]) {
    double distance = 0;
    for (size_t d = 0; d < dim; d++) {
        distance += (a[d] - b[d]) * (a[d] - b[d]);
    }
    return sqrt(distance);
}

Cluster* kMeansInit(Params *params, Example trainingSet[], unsigned int trainingSetSize, unsigned int initalId) {
    Cluster *clusters = calloc(params->k, sizeof(Cluster));
    for (size_t i = 0; i < params->k; i++) {
        clusters[i].id = initalId + i;
        clusters[i].matches = 0;
        clusters[i].center = calloc(params->dim, sizeof(double));
        clusters[i].linearSum = calloc(params->dim, sizeof(double));
        clusters[i].squareSum = calloc(params->dim, sizeof(double));
        for (size_t d = 0; d < params->dim; d++) {
            clusters[i].center[d] = trainingSet[i].val[d];
            clusters[i].linearSum[d] = 0.0;
            clusters[i].squareSum[d] = 0.0;
        }
    }
    return clusters;
}

double kMeans(Params *params, Cluster* clusters, Example trainingSet[], unsigned int trainingSetSize) {
    double improvement, prevGlobalDistance, globalDistance = params->dim * params->k * trainingSetSize * 2;
    unsigned int iteration = 0;
    do {
        prevGlobalDistance = globalDistance;
        globalDistance = 0.0;
        for (size_t i = 0; i < trainingSetSize; i++) {
            double minDist = params->dim * 2;
            Cluster *nearest = NULL;
            for (size_t k = 0; k < params->k; k++) {
                double dist = euclideanDistance(params->dim, clusters[k].center, trainingSet[i].val);
                if (nearest == NULL || dist <= minDist) {
                    minDist = dist;
                    nearest = &clusters[k];
                }
            }
            globalDistance += minDist;
            nearest->matches++;
            for (size_t d = 0; d < params->dim; d++) {
                nearest->linearSum[d] += trainingSet[i].val[d];
            }
        }
        for (size_t k = 0; k < params->k; k++) {
            for (size_t d = 0; d < params->dim; d++) {
                if (clusters[k].matches > 0)
                    clusters[k].center[d] = clusters[k].linearSum[d] / clusters[k].matches;
                clusters[k].linearSum[d] = 0.0;
                clusters[k].squareSum[d] = 0.0;
            }
            clusters[k].matches = 0;
        }
        improvement = globalDistance - prevGlobalDistance;
        fprintf(stderr, "[%3u] k-Means %le -> %le (%+le)\n", iteration, prevGlobalDistance, globalDistance, improvement);
        if (improvement < 0)
            improvement = -improvement;
        iteration++;
    } while (improvement > params->precision && iteration < 100);
    return globalDistance;
}

Cluster* clustering(Params *params, Example trainingSet[], unsigned int trainingSetSize, unsigned int initalId) {
    Cluster *clusters = kMeansInit(params, trainingSet, trainingSetSize, initalId);
    // double globalDistance =
    kMeans(params, clusters, trainingSet, trainingSetSize);
    //
    double *distances = calloc(trainingSetSize, sizeof(double));
    Cluster **matches = calloc(trainingSetSize, sizeof(Cluster *));
    for (size_t k = 0; k < params->k; k++) {
        clusters[k].radius = 0.0;
        clusters[k].matches = 0;
        clusters[k].distanceLinearSum = 0.0;
        clusters[k].distanceSquareSum = 0.0;
    }
    for (size_t i = 0; i < trainingSetSize; i++) {
        double minDist = params->dim * 2;
        Cluster *nearest = NULL;
        for (size_t k = 0; k < params->k; k++) {
            double dist = euclideanDistance(params->dim, clusters[k].center, trainingSet[i].val);
            if (nearest == NULL || dist <= minDist) {
                minDist = dist;
                nearest = &clusters[k];
            }
        }
        distances[i] = minDist;
        matches[i] = nearest;
        //
        nearest->matches++;
        nearest->distanceLinearSum += minDist;
        nearest->distanceSquareSum += minDist * minDist;
        for (size_t d = 0; d < params->dim; d++) {
            nearest->linearSum[d] += trainingSet[i].val[d];
            nearest->squareSum[d] += trainingSet[i].val[d] * trainingSet[i].val[d];
        }
    }
    for (size_t k = 0; k < params->k; k++) {
        double average = clusters[k].distanceLinearSum / clusters[k].matches;
        double variance = 0.0;
        for (size_t i = 0; i < trainingSetSize; i++) {
            if (matches[i] == &clusters[k]) {
                double p = distances[i] - average;
                variance += p * p;
            }
        }
        double stddev = sqrt(variance);
        clusters[k].radius = average + params->radiusF * stddev;
    }
    return clusters;
}

Model *training(Params *params) {
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
    while (1) {
        double *value = calloc(params->dim, sizeof(double));
        for (size_t d = 0; d < params->dim; d++) {
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
        assertDiffer(nClasses, 254);
        classesSize[l]++;
        trainingSetByClass[l] = realloc(trainingSetByClass[l], classesSize[l] * sizeof(Example));
        Example *ex = &trainingSetByClass[l][classesSize[l] -1];
        //
        ex->id = id;
        ex->val = value;
        ex->class = class;
        id++;
        // if (id > 71990) fprintf(stderr, "Ex(id=%d, val=%le, class=%c)\n", ex->id, ex->val[0], ex->class);
        //
        int hasEmptyline;
        scanf("\n%n", &hasEmptyline);
        if (hasEmptyline == 2) break;
    }
    //
    fprintf(stderr, "Training %u examples with %d classes (%s)\n", id, nClasses, classes);
    fflush(stdout);
    Model *model = calloc(1, sizeof(Model));
    model->size = 0;
    model->clusters = calloc(1, sizeof(Cluster));
    for (size_t l = 0; l < nClasses; l++) {
        Example *trainingSet = trainingSetByClass[l];
        unsigned int trainingSetSize = classesSize[l];
        char class = classes[l];
        fprintf(stderr, "Training %u examples from class %c\n", trainingSetSize, class);
        fflush(stdout);
        Cluster *clusters = clustering(params, trainingSet, trainingSetSize, model->size);
        //
        unsigned int prevSize = model->size;
        model->size += params->k;
        model->clusters = realloc(model->clusters, model->size * sizeof(Cluster));
        for (size_t k = 0; k < params->k; k++) {
            clusters[k].label = class;
            model->clusters[prevSize + k] = clusters[k];
        }
    }
    //
    return model;
}

#define UNK_LABEL '-'

Match *classify(Params *params, Model *model, Example *example, Match *match) {
    // Match *match = calloc(1, sizeof(Match));
    match->label = UNK_LABEL;
    match->cluster = NULL;
    for (size_t k = 0; k < model->size; k++) {
        double dist = euclideanDistance(params->dim, example->val, model->clusters[k].center);
        if (match->cluster == NULL || dist <= match->distance) {
            match->distance = dist;
            match->cluster = &model->clusters[k];
        }
    }
    assertDiffer(match->cluster, NULL);
    if (match->distance <= match->cluster->radius) {
        match->label = match->cluster->label;
    }
    return match;
}

int main(int argc, char const *argv[]) {
    if (argc == 2) {
        fprintf(stderr, "reading from file %s\n", argv[1]);
        stdin = fopen(argv[1], "r");
    }
    Params params;
    assertEquals(scanf("k=%d\n", &params.k), 1);
    assertEquals(scanf("dim=%d\n", &params.dim), 1);
    assertEquals(scanf("precision=%lf\n", &params.precision), 1);
    assertEquals(scanf("radiusF=%lf\n", &params.radiusF), 1);
    assertEquals(scanf("minExamplesPerCluster=%u\n", &params.minExamplesPerCluster), 1);
    fprintf(stderr, "%s\n"
                    "\tk = %d\n"
                    "\tdim = %d\n"
                    "\tprecision = %le\n"
                    "\tradiusF = %le\n"
                    "\tminExamplesPerCluster = %u\n",
            argv[0], params.k, params.dim, params.precision, params.radiusF, params.minExamplesPerCluster);

    Model *model = training(&params);

    unsigned int id = 0;
    Match match;
    Example example;
    example.val = calloc(params.dim, sizeof(double));
    printf("#id,label");
    size_t unknownsMaxSize = params.minExamplesPerCluster * params.k;
    Example *unknowns = calloc(unknownsMaxSize, sizeof(Example));
    size_t unknownsSize = 0;
    int hasEmptyline = 0;
    while (!feof(stdin) && hasEmptyline != 2) {
        for (size_t d = 0; d < params.dim; d++) {
            assertEquals(scanf("%lf,", &example.val[d]), 1);
        }
        // ignore class
        char class;
        assertEquals(scanf("%c", &class), 1);
        example.id = id;
        id++;
        scanf("\n%n", &hasEmptyline);
        //
        classify(&params, model, &example, &match);
        printf("%10u,%c\n", example.id, match.label);
        if (match.label != UNK_LABEL) continue;
        unknowns[unknownsSize] = example;
        unknowns[unknownsSize].val = calloc(params.dim, sizeof(double));
        for (size_t d = 0; d < params.dim; d++) {
            unknowns[unknownsSize].val[d] = example.val[d];
        }
        unknownsSize++;
        if (unknownsMaxSize % unknownsSize == 0) {
            unknownsMaxSize *= 2;
            unknowns = realloc(unknowns, unknownsMaxSize * sizeof(Example));
        }
        //
    }
    fprintf(stderr, "Test with %u examples\n", id);

    return EXIT_SUCCESS;
}
