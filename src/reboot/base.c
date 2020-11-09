#ifndef _BASE_C
#define _BASE_C 1

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>

#include "./base.h"

char *printableLabel(char label) {
    char *ret = calloc(20, sizeof(char));
    if (isalpha(label) || label == '-') {
        ret[0] = label;
    } else {
        sprintf(ret, "%d", label);
    }
    return ret;
}

char fromPrintableLabel(char *label) {
    char ret;
    if (isalpha(label[0]) || label[0] == '-') {
        ret = label[0];
    } else {
        int l;
        sscanf(label, "%d", &l);
        assert(l < 255);
        ret = l;
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
        assert(nClasses < 254);
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

int printCluster(int dim, Cluster *cl) {
    int count = 0;
    count += printf("Cluster: %10u, %s, %10u, %le, %le, %le",
                cl->id, printableLabel(cl->label), cl->n_matches,
                cl->distanceAvg, cl->distanceStdDev, cl->radius);
    assertNotNull(cl->center);
    for (unsigned int d = 0; d < dim; d++)
        count += printf(", %le", cl->center[d]);
    count += printf("\n");
    return count;
}

int addClusterLine(int kParam, int dim, Model *model, char lineptr[]) {
    int readCur = 0, readTot = 0;
    if (model->size > 0 && model->size % kParam == 0) {
        fprintf(stderr, "realloc %d\n", model->size);
        model->clusters = realloc(model->clusters, (model->size + kParam) * sizeof(Cluster));
    }
    Cluster *cl = &(model->clusters[model->size]);
    model->size++;
    char *labelString;
    // line is on format "Cluster: %10u, %s, %10u, %le, %le, %le" + dim * ", %le"
    int assigned = sscanf(
        lineptr, "Cluster: %10u, %m[^,], %10u, %le, %le, %le%n",
        &cl->id, &labelString, &cl->n_matches,
        &cl->distanceAvg, &cl->distanceStdDev, &cl->radius,
        &readCur);
    assertMsg(assigned == 6, "Got %d assignments.", assigned);
    readTot += readCur;
    cl->label = fromPrintableLabel(labelString);
    if (!isalpha(cl->label) && model->nextLabel <= cl->label) {
        model->nextLabel = cl->label + 1;
    }
    free(labelString);
    cl->center = calloc(dim, sizeof(double));
    for (size_t d = 0; d < dim; d++) {
        assert(sscanf(&lineptr[readTot], ", %le%n", &(cl->center[d]), &readCur));
        // fprintf(stderr, "readTot %d, remaining '%s'.\n", readTot, &lineptr[readTot]);
        readTot += readCur;
    }
    // printCluster(dim, cl);
    return readTot;
}

#endif // _BASE_C
