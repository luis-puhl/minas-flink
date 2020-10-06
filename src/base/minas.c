#ifndef _MINAS_C
#define _MINAS_C 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <math.h>
#include <time.h>
#include <ctype.h>

#include "./base.h"
#include "./minas.h"
#include "./kmeans.h"
#include "./clustream.h"

char *getModelFileName(int useCluStream, int modelSize, char *suffix) {
    char *fileName = calloc(255, sizeof(char));
    sprintf(fileName, "out/baseline-models/baseline_%s%d%s.csv", useCluStream ? "CluStream-" : "", modelSize, suffix);
    return fileName;
}

double nearestClusterValStd(Params *params, Cluster clusters[], size_t nClusters, double val[], Cluster **nearest) {
    double minDist;
    *nearest = NULL;
    for (size_t k = 0; k < nClusters; k++) {
        // double dist = 0.0;
        // for (size_t d = 0; d < params->dim; d++) {
        //     double v = (clusters[k].center[d] - val[d]);
        //     dist += v * v;
        // }
        // dist = sqrt(dist);
        double dist = euclideanDistance(params->dim, val, clusters[k].center);
        if (*nearest == NULL || dist <= minDist) {
            minDist = dist;
            *nearest = &clusters[k];
        }
    }
    return minDist;
}

double nearestClusterVal(Params *params, Cluster clusters[], size_t nClusters, double val[], Cluster **nearest) {
    double minDist;
    *nearest = NULL;
    for (size_t k = 0; k < nClusters; k++) {
        double dist = 0.0;
        for (size_t d = 0; d < params->dim; d++) {
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
    // // redo distance, because early skip
    // double dist = 0.0;
    // for (size_t d = 0; d < params->dim; d++) {
    //     double v = ((*nearest)->center[d] - val[d]);
    //     dist += v * v;
    // }
    // return sqrt(dist);
}

Example *next(Params *params, Example **reuse) {
    if (feof(stdin)){
        return NULL;
    }
    unsigned int hasEmptyline;
    scanf("\n%n", &hasEmptyline);
    if (hasEmptyline > 0) {
        return NULL;
    }
    if ((*reuse) == NULL) {
        (*reuse) = calloc(1, sizeof(Example));
        (*reuse)->id = 0;
        (*reuse)->val = calloc(params->dim, sizeof(double));
    } else {
        // increment id
        (*reuse)->id++;
    }
    int idTemp;
    if (scanf("%10u,", &idTemp) == 1) {
        (*reuse)->id = idTemp;
    }
    for (size_t d = 0; d < params->dim; d++) {
        if (scanf("%lf,", &((*reuse)->val[d])) != 1) {
            return NULL;
        }
    }
    // ignore class
    // char class;
    // assertEquals(scanf("%c", &class), 1);
    if (scanf("%*c") != 1) {
        return NULL;
    }
    return (*reuse);
}

Cluster* clustering(Params *params, Example trainingSet[], unsigned int trainingSetSize, unsigned int initalId) {
    clock_t start = clock();
    Cluster *clusters;
    if (params->useCluStream) {
        clusters = cluStream(params, trainingSet, trainingSetSize, initalId);
    } else {
        clusters = kMeansInit(params, trainingSet, trainingSetSize, initalId);
        kMeans(params, clusters, trainingSet, trainingSetSize);
    }
    //
    double *distances = calloc(trainingSetSize, sizeof(double));
    Cluster **n_matches = calloc(trainingSetSize, sizeof(Cluster *));
    for (size_t k = 0; k < params->k; k++) {
        clusters[k].radius = 0.0;
        clusters[k].n_matches = 0;
        clusters[k].distanceMax = 0.0;
        clusters[k].distanceLinearSum = 0.0;
        clusters[k].distanceSquareSum = 0.0;
    }
    for (size_t i = 0; i < trainingSetSize; i++) {
        Cluster *nearest = NULL;
        double minDist = nearestClusterVal(params, clusters, params->k, trainingSet[i].val, &nearest);
        distances[i] = minDist;
        n_matches[i] = nearest;
        //
        nearest->n_matches++;
        nearest->distanceLinearSum += minDist;
        nearest->distanceSquareSum += minDist * minDist;
        if (minDist > nearest->distanceMax) {
            nearest->distanceMax = minDist;
        }
        for (size_t d = 0; d < params->dim; d++) {
            nearest->ls_valLinearSum[d] += trainingSet[i].val[d];
            nearest->ss_valSquareSum[d] += trainingSet[i].val[d] * trainingSet[i].val[d];
        }
    }
    for (size_t k = 0; k < params->k; k++) {
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
        clusters[k].radius = params->radiusF * clusters[k].distanceStdDev;
    }
    printTiming(clustering, trainingSetSize);
    return clusters;
}

Model *training(Params *params) {
    clock_t start = clock();
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
    if (id < params->k) {
        errx(EXIT_FAILURE, "Not enough examples for training. At "__FILE__":%d\n", __LINE__);
    }
    Model *model = calloc(1, sizeof(Model));
    model->size = 0;
    model->nextLabel = '\0';
    model->clusters = calloc(1, sizeof(Cluster));
    char *fileName = getModelFileName(params->useCluStream, model->size, "-init");
    FILE *modelFile = fopen(fileName, "w");
    free(fileName);
    fprintf(modelFile, "#id, label, n_matches, distanceAvg, distanceStdDev, radius");
    //, ls_valLinearSum, ss_valSquareSum, distanceLinearSum, distanceSquareSum\n");
    for (unsigned int d = 0; d < params->dim; d++)
        fprintf(modelFile, ", c%u", d);
    fprintf(modelFile, "\n");
    for (size_t l = 0; l < nClasses; l++) {
        Example *trainingSet = trainingSetByClass[l];
        unsigned int trainingSetSize = classesSize[l];
        char class = classes[l];
        fprintf(stderr, "Training %u examples from class %c\n", trainingSetSize, class);
        Cluster *clusters = clustering(params, trainingSet, trainingSetSize, model->size);
        //
        unsigned int prevSize = model->size;
        model->size += params->k;
        model->clusters = realloc(model->clusters, model->size * sizeof(Cluster));
        for (size_t k = 0; k < params->k; k++) {
            clusters[k].label = class;
            model->clusters[prevSize + k] = clusters[k];
            //
            fprintf(modelFile, "%10u, %s, %10u, %le, %le, %le",
                    clusters[k].id, printableLabel(clusters[k].label), clusters[k].n_matches,
                    clusters[k].distanceAvg, clusters[k].distanceStdDev, clusters[k].radius);
            for (unsigned int d = 0; d < params->dim; d++)
                fprintf(modelFile, ", %le", clusters[k].center[d]);
            fprintf(modelFile, "\n");
        }
        free(clusters);
    }
    fclose(modelFile);
    printTiming(training, id);
    return model;
}

Match *identify(Params *params, Model *model, Example *example, Match *match) {
    // Match *match = calloc(1, sizeof(Match));
    match->label = UNK_LABEL;
    match->distance = nearestClusterVal(params, model->clusters, model->size, example->val, &match->cluster);
    assertDiffer(match->cluster, NULL);
    if (match->distance <= match->cluster->radius) {
        match->label = match->cluster->label;
        // // #define _USE_MOVING_CLUSTER
        // #ifdef _USE_MOVING_CLUSTER
            // match->cluster->n_matches++;
            // match->cluster->timeLinearSum += example->id;
            // match->cluster->timeSquareSum += example->id * example->id;
            // match->cluster->radius = 0.0;
            // for (size_t d = 0; d < params->dim; d++) {
            //     match->cluster->ls_valLinearSum[d] += example->val[d];
            //     match->cluster->ss_valSquareSum[d] += example->val[d] * example->val[d];
            //     //
            //     match->cluster->center[d] = match->cluster->ls_valLinearSum[d] /
            //         match->cluster->n_matches;
            //     double v = example->val[d] - match->cluster->center[d];
            //     match->cluster->radius += v * v;
            // }
            // match->cluster->radius = sqrt(match->cluster->radius / match->cluster->n_matches);
        // #endif // _USE_MOVING_CLUSTER
    }
    return match;
}

void noveltyDetection(Params *params, Model *model, Example *unknowns, size_t unknownsSize) {
    clock_t start = clock();
    char *fileName = getModelFileName(params->useCluStream, model->size, "");
    FILE *modelFile = fopen(fileName, "w");
    free(fileName);
    fprintf(modelFile, "#id, label, n_matches, distanceAvg, distanceStdDev, radius");
    //, ls_valLinearSum, ss_valSquareSum, distanceLinearSum, distanceSquareSum\n");
    for (unsigned int d = 0; d < params->dim; d++)
        fprintf(modelFile, ", c%u", d);
    fprintf(modelFile, "\n");
    //
    Cluster *clusters = clustering(params, unknowns, unknownsSize, model->size);
    int extensions = 0, novelties = 0;
    for (size_t k = 0; k < params->k; k++) {
        if (clusters[k].n_matches < params->minExamplesPerCluster) continue;
        //
        Cluster *nearest = NULL;
        double minDist = nearestClusterVal(params, model->clusters, model->size, clusters[k].center, &nearest);
        if (minDist <= params->noveltyF * nearest->distanceStdDev) {
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
        //
        fprintf(modelFile, "%10u, %s, %10u, %le, %le, %le",
                clusters[k].id, printableLabel(clusters[k].label), clusters[k].n_matches,
                clusters[k].distanceAvg, clusters[k].distanceStdDev, clusters[k].radius);
        for (unsigned int d = 0; d < params->dim; d++)
            fprintf(modelFile, ", %le", clusters[k].center[d]);
        fprintf(modelFile, "\n");
    }
    fprintf(stderr, "ND clusters: %d extensions, %d novelties\n", extensions, novelties);
    free(clusters);
    fclose(modelFile);
    printTiming(training, unknownsSize);
}

void minasOnline(Params *params, Model *model) {
    clock_t start = clock();
    unsigned int id = 0;
    Match match;
    Example example;
    example.val = calloc(params->dim, sizeof(double));
    printf("#pointId,label\n");
    size_t unknownsMaxSize = params->minExamplesPerCluster * params->k;
    size_t noveltyDetectionTrigger = params->minExamplesPerCluster * params->k;
    Example *unknowns = calloc(unknownsMaxSize, sizeof(Example));
    size_t unknownsSize = 0;
    size_t lastNDCheck = 0;
    int hasEmptyline = 0;
    fprintf(stderr, "Taking test stream from stdin\n");
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
        printf("%10u,%s\n", example.id, printableLabel(match.label));
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
        if (unknownsSize % noveltyDetectionTrigger == 0 && id - lastNDCheck > noveltyDetectionTrigger) {
            lastNDCheck = id;
            unsigned int prevSize = model->size;
            noveltyDetection(params, model, unknowns, unknownsSize);
            unsigned int nNewClusters = model->size - prevSize;
            //
            size_t reclassified = 0;
            for (size_t ex = 0; ex < unknownsSize; ex++) {
                // compress
                unknowns[ex - reclassified] = unknowns[ex];
                // identify(params, model, &unknowns[ex], &match);
                // match.label = UNK_LABEL;
                // use only new clusters
                Cluster *nearest;
                double distance = nearestClusterVal(params, &model->clusters[prevSize], nNewClusters, unknowns[ex].val, &nearest);
                // match.distance = nearestClusterVal(params, model->clusters, model->size, unknowns[ex].val, &match.cluster);
                assertDiffer(nearest, NULL);
                if (distance <= nearest->distanceMax) {
                    if (params->useReclassification) {
                        printf("%10u,%s\n", unknowns[ex].id, printableLabel(nearest->label));
                    }
                    reclassified++;
                }
                // if (match.label == UNK_LABEL)
                //     continue;
                // printf("%10u,%s\n", unknowns[ex].id, printableLabel(match.label));
                // reclassified++;
            }
            fprintf(stderr, "Reclassified %lu\n", reclassified);
            unknownsSize -= reclassified;
        }
    }
    fprintf(stderr, "Final flush %lu\n", unknownsSize);
    // final flush
    if (unknownsSize > params->k) {
        unsigned int prevSize = model->size;
        noveltyDetection(params, model, unknowns, unknownsSize);
        unsigned int nNewClusters = model->size - prevSize;
        //
        size_t reclassified = 0;
        for (size_t ex = 0; ex < unknownsSize; ex++) {
            // compress
            unknowns[ex - reclassified] = unknowns[ex];
            Cluster *nearest;
            double distance = nearestClusterVal(params, &model->clusters[prevSize], nNewClusters, unknowns[ex].val, &nearest);
            assertDiffer(nearest, NULL);
            if (distance <= nearest->distanceMax) {
                printf("%10u,%s\n", unknowns[ex].id, printableLabel(nearest->label));
                reclassified++;
            }
        }
        fprintf(stderr, "Reclassified %lu\n", reclassified);
        unknownsSize -= reclassified;
    }
    printTiming(minasOnline, id);
    //
    char *fileName = getModelFileName(params->useCluStream, model->size, "-final");
    FILE *modelFile = fopen(fileName, "w");
    free(fileName);
    fprintf(modelFile, "#id, label, n_matches, distanceAvg, distanceStdDev, radius");
    //, ls_valLinearSum, ss_valSquareSum, distanceLinearSum, distanceSquareSum\n");
    for (unsigned int d = 0; d < params->dim; d++)
        fprintf(modelFile, ", c%u", d);
    fprintf(modelFile, "\n");
    for (size_t k = 0; k < model->size; k++) {
        Cluster *cl = &model->clusters[k];
        fprintf(modelFile, "%10u, %s, %10u, %le, %le, %le",
                cl->id, printableLabel(cl->label), cl->n_matches,
                cl->distanceAvg, cl->distanceStdDev, cl->radius);
        for (unsigned int d = 0; d < params->dim; d++)
            fprintf(modelFile, ", %le", cl->center[d]);
        fprintf(modelFile, "\n");
    }
    fclose(modelFile);
}

#endif //_MINAS_C