#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <math.h>
#include <time.h>

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
    const char *executable;
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
    double distanceAvg, distanceStdDev, radius;
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
    clock_t start = clock();
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
    printTiming(trainingSetSize);
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
        if (clusters[k].matches == 0) continue;
        clusters[k].distanceAvg = clusters[k].distanceLinearSum / clusters[k].matches;
        clusters[k].distanceStdDev = 0.0;
        for (size_t i = 0; i < trainingSetSize; i++) {
            if (matches[i] == &clusters[k]) {
                double p = distances[i] - clusters[k].distanceAvg;
                clusters[k].distanceStdDev += p * p;
            }
        }
        clusters[k].distanceStdDev = sqrt(clusters[k].distanceStdDev);
        clusters[k].radius = clusters[k].distanceAvg + params->radiusF * clusters[k].distanceStdDev;
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
    FILE *modelFile = fopen("out/baseline-models/baseline_0.csv", "w");
    fprintf(modelFile, "#id, label, matches, distanceAvg, distanceStdDev, radius");
    //, linearSum, squareSum, distanceLinearSum, distanceSquareSum\n");
    for (unsigned int d = 0; d < params->dim; d++)
        fprintf(modelFile, ", c%u", d);
    fprintf(modelFile, "\n");
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
            //
            fprintf(modelFile, "%10u, %c, %10u, %le, %le, %le",
                    clusters[k].id, clusters[k].label, clusters[k].matches,
                    clusters[k].distanceAvg, clusters[k].distanceStdDev, clusters[k].radius);
            for (unsigned int d = 0; d < params->dim; d++)
                fprintf(modelFile, ", %le", clusters[k].center[d]);
            fprintf(modelFile, "\n");
        }
        free(clusters);
    }
    fclose(modelFile);
    //
    return model;
}

#define UNK_LABEL '-'

Match *identify(Params *params, Model *model, Example *example, Match *match) {
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

void minasOnline(Params *params, Model *model) {
    clock_t start = clock();
    unsigned int id = 0;
    Match match;
    Example example;
    example.val = calloc(params->dim, sizeof(double));
    printf("#id,label\n");
    size_t unknownsMaxSize = params->minExamplesPerCluster * params->k;
    size_t noveltyDetectionTrigger = params->minExamplesPerCluster * params->k;
    Example *unknowns = calloc(unknownsMaxSize, sizeof(Example));
    size_t unknownsSize = 0;
    int hasEmptyline = 0;
    while (!feof(stdin) && hasEmptyline != 2) {
        for (size_t d = 0; d < params->dim; d++) {
            assertEquals(scanf("%lf,", &example.val[d]), 1);
        }
        // ignore class
        char class;
        assertEquals(scanf("%c", &class), 1);
        example.id = id;
        id++;
        scanf("\n%n", &hasEmptyline);
        //
        identify(params, model, &example, &match);
        printf("%10u,%c\n", example.id, match.label);
        //
        if (match.label != UNK_LABEL) continue;
        unknowns[unknownsSize] = example;
        unknowns[unknownsSize].val = calloc(params->dim, sizeof(double));
        for (size_t d = 0; d < params->dim; d++) {
            unknowns[unknownsSize].val[d] = example.val[d];
        }
        unknownsSize++;
        if (unknownsSize >= unknownsMaxSize) {
            unknownsMaxSize *= 2;
            unknowns = realloc(unknowns, unknownsMaxSize * sizeof(Example));
        }
        //
        if (unknownsSize % noveltyDetectionTrigger == 0) {
            fprintf(stderr, "Novelty Detection with %lu examples\n", unknownsSize);
        }
    }
    printTiming(id);
}

int main(int argc, char const *argv[]) {
    if (argc == 2) {
        fprintf(stderr, "reading from file %s\n", argv[1]);
        stdin = fopen(argv[1], "r");
    }
    Params params;
    params.executable = argv[0];
    fprintf(stderr, "%s\n", params.executable);
    assertEquals(scanf("k=%d\n", &params.k), 1);
    assertEquals(scanf("dim=%d\n", &params.dim), 1);
    assertEquals(scanf("precision=%lf\n", &params.precision), 1);
    assertEquals(scanf("radiusF=%lf\n", &params.radiusF), 1);
    assertEquals(scanf("minExamplesPerCluster=%u\n", &params.minExamplesPerCluster), 1);
    fprintf(stderr, "\tk = %d\n", params.k);
    fprintf(stderr, "\tdim = %d\n", params.dim);
    fprintf(stderr, "\tprecision = %le\n", params.precision);
    fprintf(stderr, "\tradiusF = %le\n", params.radiusF);
    fprintf(stderr, "\tminExamplesPerCluster = %u\n", params.minExamplesPerCluster);

    Model *model = training(&params);

    minasOnline(&params, model);

    return EXIT_SUCCESS;
}
