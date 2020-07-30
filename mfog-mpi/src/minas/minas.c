#ifndef MINAS_C
#define MINAS_C

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <err.h>

#include "minas.h"
#include "../util/loadenv.h"
#include "../util/kMeans.h"

// #define SQR_DISTANCE 1
#define line_len 10 * 1024

double MNS_distance(double a[], double b[], int dimension) {
    double distance = 0;
    for (int i = 0; i < dimension; i++) {
        double diff = a[i] - b[i];
        distance += diff * diff;
    }
    #ifdef SQR_DISTANCE
        return distance;
    #else
        return sqrt(distance);
    #endif // SQR_DISTANCE
}

Model* readModel(int dimension, FILE *file, FILE *timing, char *executable) {
    if (file == NULL) errx(EXIT_FAILURE, "bad file");
    clock_t start = clock();
    char line[line_len + 1];
    //
    Model *model = malloc(sizeof(Model));
    model->size = 0;
    model->dimension = dimension;
    model->nextNovelty = 'a';
    model->vals = malloc(1 * sizeof(Cluster));
    int lines = 0;
    //
    while (fgets(line, line_len, file)) {
        lines++;
        if (line[0] == '#') continue;
        model->vals = realloc(model->vals, (++model->size) * sizeof(Cluster));
        Cluster *cl = &(model->vals[model->size - 1]);
        cl->center = malloc(dimension * sizeof(double));
        // #id,label,category,matches,time,meanDistance,radius,c0,c1,c2,c3,c4,c5,c6,c7,c8,c9,c10,c11,c12,c13,c14,c15,c16,c17,c18,c19,c20,c21
        int assigned = 0;
        sscanf(line, "%d,%c,%c,%d,%d,%lf,%lf,", &cl->id, &cl->label, &cl->category, &cl->matches, &cl->time, &cl->meanDistance, &cl->radius);
        for (int d = 0; d < dimension - 1; d++) {
            assigned += sscanf(line, "%lf,", &cl->center[d]);
        }
        assigned += sscanf(line, "%lf\n", &cl->center[dimension - 1]);
        if (assigned != (7 + dimension)) {
            errx(EXIT_FAILURE, "File with wrong format. On line %d '%s'" __FILE__ ":%d\n", lines, line, __LINE__);
        }
    }
    if (timing) {
        PRINT_TIMING(timing, executable, 1, start, model->size);
    }
    return model;
}

void writeModel(FILE *file, Model *model, FILE *timing, char *executable) {
    int dimension = model->dimension;
    if (file == NULL) errx(EXIT_FAILURE, "bad file");
    clock_t start = clock();
    //
    // #id,label,category,matches,time,meanDistance,radius,c0,c1,c2,c3,c4,c5,c6,c7,c8,c9,c10,c11,c12,c13,c14,c15,c16,c17,c18,c19,c20,c21
    fprintf(file, "#id,label,category,matches,time,meanDistance,radius,");
    for (int d = 0; d < dimension - 1; d++) {
        fprintf(file, "c%d,", d);
    }
    fprintf(file, "c%d\n", dimension - 1);
    //
    for (int i = 0; i < model->size; i++) {
        Cluster *cl = &(model->vals[i]);
        fprintf(file, "%d,%c,%c,%d,%d,%le,%le,", cl->id, cl->label, cl->category, cl->matches, cl->time, cl->meanDistance, cl->radius);
        for (int d = 0; d < dimension -1; d++) {
            fprintf(file, "%le,", cl->center[d]);
        }
        fprintf(file, "%le\n", cl->center[dimension - 1]);
    }
    fflush(file);
    if (timing) {
        PRINT_TIMING(timing, executable, 1, start, model->size);
    }
}

Point *readExamples(int dimension, FILE *file, int *nExamples, FILE *timing, char *executable) {
    if (file == NULL || ferror(file)) errx(EXIT_FAILURE, "bad file");
    clock_t start = clock();
    char line[line_len + 1];
    //
    Point *exs;
    exs = malloc(1 * sizeof(Point));
    //
    // for (examples)
    *nExamples = 0;
    int isFileIndexed = 0;
    while (fgets(line, line_len, file)) {
        if (line[0] == '#') {
            // printf("%s", line);
            if (line[1] == 'i' && line[2] == 'd') {
                isFileIndexed = 1;
                // printf("File is indexed. At "__FILE__":%d\n", __LINE__);
            }
            continue;
        }
        // 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,A
        Point *ex = &(exs[*nExamples]);
        ex->value = malloc(dimension * sizeof(double));
        int assigned = 0;
        char *linePtr = line;
        if (isFileIndexed) {
            assigned += sscanf(linePtr, "%d,", &ex->id);
            for (;*linePtr != ','; linePtr++) { /* nope */ }
            linePtr++;
        } else {
            ex->id = *nExamples;
        }
        // printf("%s\tPoint(id=%d, val=[", line, ex->id);
        for (int d = 0; d < dimension; d++) {
            assigned += sscanf(linePtr, "%lf,", &ex->value[d]);
            // printf("%lf, ", ex->value[d]);
            for (;*linePtr != ','; linePtr++) { /* nope */ }
            linePtr++;
        }
        assigned += sscanf(linePtr, "%c\n", &ex->label);
        // printf("], class='%c')\n", ex->label);
        if (assigned != (1 + isFileIndexed + dimension)) {
            errx(EXIT_FAILURE, "File with wrong format. On line %d '%s'" __FILE__ ":%d\n", (*nExamples), line, __LINE__);
        }
        //
        (*nExamples)++;
        exs = realloc(exs, (*nExamples + 1) * sizeof(Point));
    }
    // fclose(file);
    exs[*nExamples].id = -1;
    exs[*nExamples].value = NULL;
    exs[*nExamples].label = '\0';
    if (timing) {
        PRINT_TIMING(timing, executable, 1, start, (*nExamples +1));
    }
    return exs;
}

void classify(int dimension, Model *model, Point *ex, Match *match) {
    match->distance = (double) dimension;
    match->pointId = ex->id;
    match->label = '-';
    // printf("#pid_%d", ex->id);
    for (int i = 0; i < model->size; i++) {
        double distance = MNS_distance(ex->value, model->vals[i].center, dimension);
        // printf("%le,", distance);
        // allDistances[i] = distance;
        if (distance <= match->distance) {
            // match->cluster = &(model->vals[i]);
            match->clusterId = model->vals[i].id;
            match->clusterLabel = model->vals[i].label;
            match->clusterCatergoy = model->vals[i].category;
            match->clusterRadius = model->vals[i].radius;
            match->secondDistance = match->distance;
            match->distance = distance;
        } else if (distance <= match->secondDistance) {
            match->secondDistance = distance;
        }
    }
    // printf("\n");
    // If the border isn't included, a novelty cluster would miss the farthest
    // example that was used to create the cluster in the first place.
    // if (match->distance < match->clusterRadius) {
    if (match->distance <= match->clusterRadius) {
        match->label = match->clusterLabel;
    }

    // printf("%d,%c,%d,%c,%e,%e\n",
    //     match->pointId, match->isMatch, match->clusterId,
    //     match->label, match->distance, match->radius
    // );
}

Cluster *fillCluster(int dimension, int k, Cluster clusters[], int nExamples, Point examples[], FILE *timing, char *executable, Model *model) {
    clock_t start = clock();
    // update distances
    for (int i = 0; i < k; i++) {
        clusters[i].matches = 0;
        clusters[i].distancesMax = 0.0;
        clusters[i].meanDistance = 0.0;
        clusters[i].radius = 0.0;
        for (int d = 0; d < dimension; d++) {
            clusters[i].pointSum[d] = 0.0;
            clusters[i].pointSqrSum[d] = 0.0;
        }
    }
    for (int exIndx = 0; exIndx < nExamples; exIndx++) {
        // classify(dimension, model, &(group[i]), &m);
        Point *ex = &(examples[exIndx]);
        double nearestDistance;
        Cluster *nearest = NULL;
        if (model != NULL) {
            for (int clIndx = 0; clIndx < model->size; clIndx++) {
                Cluster *cl = &(model->vals[clIndx]);
                double distance = MNS_distance(ex->value, cl->center, dimension);
                if (nearest == NULL || nearestDistance > distance) {
                    nearest = cl;
                    nearestDistance = distance;
                }
            }
        }
        for (int clIndx = 0; clIndx < k; clIndx++) {
            Cluster *cl = &(clusters[clIndx]);
            double distance = MNS_distance(ex->value, cl->center, dimension);
            if (nearest == NULL || nearestDistance > distance) {
                nearest = cl;
                nearestDistance = distance;
            }
        }
        nearest->matches++;
        if (nearestDistance > nearest->distancesMax) {
            nearest->distancesMax = nearestDistance;
        }
        nearest->distancesSum += nearestDistance;
        nearest->distancesSqrSum += nearestDistance * nearestDistance;
        for (int d = 0; d < dimension; d++) {
            nearest->pointSum[d] += ex->value[d];
            nearest->pointSqrSum[d] += ex->value[d] * ex->value[d];
        }
    }
    for (int clIdx = 0; clIdx < k; clIdx++) {
        Cluster *cl = &clusters[clIdx];
        if (cl->matches != 0) {
            cl->meanDistance = cl->distancesSum / cl->matches;
        }
        // cl->radius = 0;
        cl->radius = cl->distancesMax;
    }
    if (timing) {
        PRINT_TIMING(timing, executable, 1, start, nExamples);
    }
    return clusters;
}

/**
 * Initial training
**/
Model* MNS_offline(int nExamples, Point examples[], int k, int dimension, FILE *timing, char *executable) {
    // one kMeans per label
    clock_t start = clock();
    Model *model = malloc(sizeof(Model));
    model->size = 0;
    model->dimension = dimension;
    model->nextNovelty = 'a';
    model->vals = malloc(1 * sizeof(Cluster));
    //
    int labelsSize = 0;
    char *labels = malloc(20 * sizeof(char));
    int *groupSizes = malloc(20 * sizeof(int));
    Point ***groups = malloc(20 * sizeof(Point*));
    for (int i = 0; i < 20; i++) {
        labels[i] = '\0';
    }
    for (int i = 0; i < nExamples; i++) {
        for (int l = 0; l < 140; l++) {
            if (labels[l] == '\0') {
                labelsSize++;
                labels[l] = examples[i].label;
                groups[l] = malloc(nExamples * sizeof(Point*));
                groupSizes[l] = 0;
            }
            if (labels[l] == examples[i].label) {
                groups[l][groupSizes[l]] = &(examples[i]);
                groupSizes[l]++;
                // next example, reset label
                break;
            }
        }
    }
    printf("Labels %s \n", labels);
    char category = 'n';
    for (int l = 0; l < labelsSize; l++) {
        char label = labels[l];
        int groupSize = groupSizes[l];
        Point **group = groups[l];
        Point *linearGroup = malloc(groupSize * sizeof(Point));
        printf("clustering label %c with %5d examples\n", label, groupSize);
        for (int g = 0; g < groupSize; g++) {
            linearGroup[g] = *group[g];
        }
        
        if (groupSize < k) {
            errx(EXIT_FAILURE, "Not enough examples for clustering. Needed %d and got %d. At "__FILE__":%d\n", k, groupSize, __LINE__);
        }
        //
        int prevModelSize = model->size;
        model->size += k;
        printf("realloc from %d to %d\n", prevModelSize, model->size);
        model->vals = realloc(model->vals, model->size * sizeof(Cluster));
        //
        Cluster *clusters = &(model->vals[prevModelSize]);
        clusters = kMeansInit(dimension, k, clusters, groupSize, linearGroup, prevModelSize, label, category, timing, executable);
        clusters = kMeans(dimension, k, clusters, groupSize, linearGroup, timing, executable);
        clusters = fillCluster(dimension, k, clusters, groupSize, linearGroup, timing, executable, NULL);
        //
        free(linearGroup);
    }
    //
    for (int l = 0; l < labelsSize; l++) {
        free(groups[l]);
    }
    free(labels);
    free(groupSizes);
    free(groups);
    
    // dont free(model)
    // dont free(vals)
    if (timing) {
        PRINT_TIMING(timing, executable, 1, start, nExamples);
    }
    return model;
}


Model *noveltyDetection(int k, Model *model, int unknownsSize, Point unknowns[], int minExCluster, double noveltyThreshold, FILE *timing, char *executable) {
    clock_t start = clock();
    char label = 'a';
    char category = 'u';
    int dimension = model->dimension;
    Cluster *clusters = malloc(k * sizeof(Cluster));

    clusters = kMeansInit(dimension, k, clusters, unknownsSize, unknowns, model->size, label, category, timing, executable);
    clusters = kMeans(dimension, k, clusters, unknownsSize, unknowns, timing, executable);
    clusters = fillCluster(dimension, k, clusters, unknownsSize, unknowns, timing, executable, model);

    for (int clId = 0; clId < k; clId++) {
        if (clusters[clId].matches >= minExCluster) {
            printf("\tNew pattern in %5d examples. ", clusters[clId].matches);
            double minDist = model->dimension;
            Cluster *nearest = NULL;
            for (int i = 0; i < model->size; i++) {
                double dist = MNS_distance(model->vals[i].center, clusters[clId].center, model->dimension);
                if (dist < minDist || nearest == NULL) {
                    nearest = &(model->vals[i]);
                    minDist = dist;
                }
            }
            // double ndThreshold = nearest->meanDistance * noveltyThreshold;
            double ndThreshold = nearest->radius * noveltyThreshold;
            printf("Nearest cluster in model %e distant and ndThreshold=%e. ", minDist, ndThreshold);
            if (minDist < ndThreshold) {
                // cluster in an extension of a concept
                printf("Is extension of '%c' concept.\n", nearest->label);
                clusters[clId].label = nearest->label;
                clusters[clId].category = 'e';
            } else {
                // cluster is a novelty pattern
                printf("Will call this novelty '%c'.\n", model->nextNovelty);
                clusters[clId].label = model->nextNovelty;
                clusters[clId].category = 'a';
                model->nextNovelty++;
                if (model->nextNovelty == 'z' + 1) {
                    model->nextNovelty = '0';
                }
                if (model->nextNovelty == '9' + 1) {
                    model->nextNovelty = 'a';
                }
            }
            //
            int prevSize = model->size;
            model->size++;
            model->vals = realloc(model->vals, model->size * sizeof(Cluster));
            model->vals[prevSize] = clusters[clId];
        } else {
            free(clusters[clId].center);
            free(clusters[clId].pointSum);
            free(clusters[clId].pointSqrSum);
        }
    }

    if (timing) {
        PRINT_TIMING(timing, executable, 1, start, unknownsSize);
    }
    return model;
}

#endif // MINAS_C
