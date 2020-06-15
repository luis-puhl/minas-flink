#ifndef MINAS_FUNCS
#define MINAS_FUNCS

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <err.h>

#include "minas.h"
#include "loadenv.h"

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

Model *kMeansInit(int nClusters, int dimension, Point examples[]) {
    Model *model = malloc(sizeof(Model));
    model->size = nClusters;
    model->dimension = dimension;
    model->vals = malloc(model->size * sizeof(Cluster));
    for (int i = 0; i < model->size; i++) {
        model->vals[i].id = i;
        model->vals[i].center = malloc(model->dimension * sizeof(double));
        for (int j = 0; j < model->dimension; j++) {
            model->vals[i].center[j] = examples[i].value[j];
        }
        model->vals[i].label = examples[i].label;
        model->vals[i].category = 'n';
        model->vals[i].time = 0;
        model->vals[i].matches = 0;
        model->vals[i].meanDistance = 0.0;
        model->vals[i].radius = 0.0;
    }
    return model;
}

Model *kMeans(Model *model, int nClusters, int dimension, Point examples[], int nExamples, FILE *timing, char *executable) {
    // Model *model = kMeansInit(nClusters, dimension, examples);
    Match match;
    clock_t start = clock();
    //
    double globalDistance = dimension * 2.0, prevGlobalDistance, diffGD = 1.0;
    double newCenters[nClusters][dimension], distances[nClusters], sqrDistances[nClusters];
    int maxIterations = 10;
    while (diffGD > 0.00001 && maxIterations-- > 0) {
        // setup
        prevGlobalDistance = globalDistance;
        globalDistance = 0.0;
        for (int i = 0; i < model->size; i++) {
            distances[model->vals[i].id] = 0.0;
            sqrDistances[model->vals[i].id] = 0.0;
            for (int d = 0; d < dimension; d++) {
                newCenters[model->vals[i].id][d] = 0.0;
            }
        }
        // distances
        for (int i = 0; i < nExamples; i++) {
            classify(dimension, model, &(examples[i]), &match);
            globalDistance += match.distance;
            distances[match.cluster->id] += match.distance;
            sqrDistances[match.cluster->id] += match.distance * match.distance;
            for (int d = 0; d < dimension; d++) {
                newCenters[match.cluster->id][d] += examples[i].value[d];
            }
            match.cluster->matches++;
        }
        // new centers and radius
        for (int i = 0; i < model->size; i++) {
            Cluster *cl = &(model->vals[i]);
            // skip clusters that didn't move
            if (cl->matches == 0) continue;
            cl->time++;
            // avg of examples in the cluster
            double maxDistance = -1.0;
            for (int d = 0; d < dimension; d++) {
                cl->center[d] = newCenters[cl->id][d] / cl->matches;
                if (distances[cl->id] > maxDistance) {
                    maxDistance = distances[cl->id];
                }
            }
            cl->meanDistance = distances[cl->id] / cl->matches;
            /**
             * Radius is not clearly defined in the papers and original source code
             * So here is defined as max distance
             *  OR square distance sum divided by matches.
             **/
            // cl->radius = sqrDistances[cl->id] / cl->matches;
            cl->radius = maxDistance;
        }
        //
        diffGD = globalDistance / prevGlobalDistance;
        fprintf(stderr, "%s iter=%d, diff%%=%e (%e -> %e)\n", __FILE__, maxIterations, diffGD, prevGlobalDistance, globalDistance);
    }
    if (timing) {
        PRINT_TIMING(timing, executable, 1, start);
    }
    return model;
}

void readModel(int dimension, FILE *file, Model *model, FILE *timing, char *executable) {
    if (file == NULL) errx(EXIT_FAILURE, "bad file");
    clock_t start = clock();
    char line[line_len + 1];
    //
    model->size = 0;
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
        int assigned = sscanf(line,
            "%d,%c,%c,"
            "%d,%d,%lf,%lf,"
            "%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf\n",
            &cl->id, &cl->label, &cl->category,
            &cl->matches, &cl->time, &cl->meanDistance, &cl->radius,
            &cl->center[0], &cl->center[1], &cl->center[2], &cl->center[3], &cl->center[4],
            &cl->center[5], &cl->center[6], &cl->center[7], &cl->center[8], &cl->center[9],
            &cl->center[10], &cl->center[11], &cl->center[12], &cl->center[13], &cl->center[14],
            &cl->center[15], &cl->center[16], &cl->center[17], &cl->center[18], &cl->center[19],
            &cl->center[20], &cl->center[21]
        );
        #ifdef SQR_DISTANCE
            cl->radius *= cl->radius;
        #endif // SQR_DISTANCE
        if (assigned != 29) errx(EXIT_FAILURE, "File with wrong format. On line %d '%s'", lines, line);
    }
    fclose(file);
    if (timing) {
        PRINT_TIMING(timing, executable, 1, start);
    }
}

Point *readExamples(int dimension, FILE *file, int *nExamples, FILE *timing, char *executable) {
    if (file == NULL) errx(EXIT_FAILURE, "bad file");
    clock_t start = clock();
    char line[line_len + 1];
    //
    Point *exs;
    unsigned int exSize = 1;
    exs = malloc(exSize * sizeof(Point));
    //
    // for (examples)
    (*nExamples) = 0;
    while (fgets(line, line_len, file)) {
        (*nExamples)++;
        if (line[0] == '#') continue;
        // 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,A
        Point *ex = &(exs[exSize-1]);
        ex->value = malloc(dimension * sizeof(double));
        int assigned = sscanf(line,
            "%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,"
            "%c\n",
            &ex->value[0], &ex->value[1], &ex->value[2], &ex->value[3], &ex->value[4],
            &ex->value[5], &ex->value[6], &ex->value[7], &ex->value[8], &ex->value[9],
            &ex->value[10], &ex->value[11], &ex->value[12], &ex->value[13], &ex->value[14],
            &ex->value[15], &ex->value[16], &ex->value[17], &ex->value[18], &ex->value[19],
            &ex->value[20], &ex->value[21], &ex->label
        );
        ex->id = exSize;
        if (assigned != 23) errx(EXIT_FAILURE, "File with wrong format. On line %d '%s'", (*nExamples), line);
        //
        exs = realloc(exs, (++exSize) * sizeof(Point));
    }
    fclose(file);
    Point *ex = &(exs[exSize-1]);
    ex->id = -1;
    ex->value = NULL;
    ex->label = '\0';
    if (timing) {
        PRINT_TIMING(timing, executable, 1, start);
    }
    return exs;
}

void classify(int dimension, Model *model, Point *ex, Match *match) {
    match->distance = (double) dimension;
    match->pointId = ex->id;
    for (int i = 0; i < model->size; i++) {
        double distance = MNS_distance(ex->value, model->vals[i].center, dimension);
        if (match->distance > distance) {
            match->cluster = &(model->vals[i]);
            match->distance = distance;
        }
    }
    match->isMatch = match->distance <= match->cluster->radius ? 'y' : 'n';
    
    // printf("%d,%c,%d,%c,%e,%e\n",
    //     match->pointId, match->isMatch, match->clusterId,
    //     match->label, match->distance, match->radius
    // );
}

#endif // MINAS_FUNCS
