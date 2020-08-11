#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <math.h>
#include <time.h>
#include <ctype.h>

// #define SQR_DIST 1

#include "./base.h"
#include "./kmeans.h"
#include "./clustream.h"

Cluster* clustering(Params *params, Example trainingSet[], unsigned int trainingSetSize, unsigned int initalId) {
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
        clusters[k].distanceLinearSum = 0.0;
        clusters[k].distanceSquareSum = 0.0;
    }
    for (size_t i = 0; i < trainingSetSize; i++) {
        double minDist;
        Cluster *nearest = NULL;
        for (size_t k = 0; k < params->k; k++) {
            #ifdef SQR_DIST
            double dist = euclideanSqrDistance(params->dim, clusters[k].center, trainingSet[i].val);
            #else
            double dist = euclideanDistance(params->dim, clusters[k].center, trainingSet[i].val);
            #endif
            if (nearest == NULL || dist <= minDist) {
                minDist = dist;
                nearest = &clusters[k];
            }
        }
        #ifdef SQR_DIST
        minDist = sqrt(minDist);
        #endif
        distances[i] = minDist;
        n_matches[i] = nearest;
        //
        nearest->n_matches++;
        nearest->distanceLinearSum += minDist;
        nearest->distanceSquareSum += minDist * minDist;
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
        clusters[k].radius = clusters[k].distanceAvg + params->radiusF * clusters[k].distanceStdDev;
    }
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
    Model *model = calloc(1, sizeof(Model));
    model->size = 0;
    model->nextLabel = '\0';
    model->clusters = calloc(1, sizeof(Cluster));
    FILE *modelFile;
    if (params->useCluStream) {
        modelFile = fopen("out/baseline-CluStream-trainign.csv", "w");
    } else {
        modelFile = fopen("out/baseline-trainign.csv", "w");
    }
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
    printTiming(id);
    return model;
}

Match *identify(Params *params, Model *model, Example *example, Match *match) {
    // Match *match = calloc(1, sizeof(Match));
    match->label = UNK_LABEL;
    match->cluster = NULL;
    for (size_t k = 0; k < model->size; k++) {
        #ifdef SQR_DIST
        double dist = euclideanSqrDistance(params->dim, example->val, model->clusters[k].center);
        #else
        double dist = euclideanDistance(params->dim, example->val, model->clusters[k].center);
        #endif
        if (match->cluster == NULL || dist <= match->distance) {
            match->distance = dist;
            match->cluster = &model->clusters[k];
        }
    }
    assertDiffer(match->cluster, NULL);
    #ifdef SQR_DIST
    match->distance = sqrt(match->distance);
    #endif
    if (match->distance <= match->cluster->radius) {
        match->label = match->cluster->label;
    }
    return match;
}

void noveltyDetection(Params *params, Model *model, Example *unknowns, size_t unknownsSize) {
    clock_t start = clock();
    char fileName[255];
    sprintf(fileName, "out/baseline-%smodels/baseline_%d.csv", params->useCluStream ? "CluStream-" : "", model->size);
    FILE *modelFile = fopen(fileName, "w");
    fprintf(modelFile, "#id, label, n_matches, distanceAvg, distanceStdDev, radius");
    //, ls_valLinearSum, ss_valSquareSum, distanceLinearSum, distanceSquareSum\n");
    for (unsigned int d = 0; d < params->dim; d++)
        fprintf(modelFile, ", c%u", d);
    fprintf(modelFile, "\n");
    //
    Cluster *clusters = clustering(params, unknowns, unknownsSize, model->size);
    for (size_t k = 0; k < params->k; k++) {
        if (clusters[k].n_matches < params->minExamplesPerCluster) continue;
        //
        double minDist;
        Cluster *nearest = NULL;
        for (size_t i = 0; i < model->size; i++) {
            #ifdef SQR_DIST
            double dist = euclideanSqrDistance(params->dim, clusters[k].center, model->clusters[i].center);
            #else
            double dist = euclideanDistance(params->dim, clusters[k].center, model->clusters[i].center);
            #endif
            if (nearest == NULL || dist < minDist) {
                minDist = dist;
                nearest = &model->clusters[i];
            }
        }
        #ifdef SQR_DIST
        minDist = sqrt(minDist);
        #endif
        if (minDist <= nearest->distanceAvg + params->noveltyF * nearest->distanceStdDev) {
            clusters[k].label = nearest->label;
        } else {
            clusters[k].label = model->nextLabel;
            // inc label
            model->nextLabel = (model->nextLabel + 1) % 255;
            // fprintf(stderr, "Novelty %s\n", printableLabel(clusters[k].label));
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
    free(clusters);
    fclose(modelFile);
    printTiming((int) unknownsSize);
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
            noveltyDetection(params, model, unknowns, unknownsSize);
            //
            size_t reclassified = 0;
            for (size_t ex = 0; ex < unknownsSize; ex++) {
                identify(params, model, &unknowns[ex], &match);
                // compress
                unknowns[ex - reclassified] = unknowns[ex];
                if (match.label == UNK_LABEL)
                    continue;
                printf("%10u,%s\n", unknowns[ex].id, printableLabel(match.label));
                reclassified++;
            }
            fprintf(stderr, "Reclassified %lu\n", reclassified);
            unknownsSize -= reclassified;
        }
    }
    // final flush
    noveltyDetection(params, model, unknowns, unknownsSize);
    size_t reclassified = 0;
    for (size_t ex = 0; ex < unknownsSize; ex++) {
        identify(params, model, &unknowns[ex], &match);
        // compress
        unknowns[ex - reclassified] = unknowns[ex];
        if (match.label == UNK_LABEL)
            continue;
        printf("%10u,%s\n", unknowns[ex].id, printableLabel(match.label));
        reclassified++;
    }
    fprintf(stderr, "Final flush %lu\n", reclassified);
    printTiming(id);
    //
    FILE *modelFile;
    if (params->useCluStream) {
        modelFile = fopen("out/baseline-CluStream-models/baseline_final.csv", "w");
    } else {
        modelFile = fopen("out/baseline-models/baseline_final.csv", "w");
    }
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
