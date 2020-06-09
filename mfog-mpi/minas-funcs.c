#ifndef MINAS_FUNCS
#define MINAS_FUNCS

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <err.h>

#include "./minas.h"

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

void readModel(int dimension, char *modelName, Model *model) {
    clock_t start = clock();
    char line[line_len + 1];
    //
    model->size = 0;
    model->vals = malloc(1 * sizeof(Cluster));
    //
    FILE *file;
    fprintf(stderr, "Reading model from    '%s'\n", modelName);
    file = fopen(modelName, "r");
    if (file == NULL) errx(EXIT_FAILURE, "bad file open '%s'", modelName);
    while (fgets(line, line_len, file)) {
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
        if (assigned != 29) errx(EXIT_FAILURE, "File with wrong format  '%s'", modelName);
    }
    fclose(file);
    fprintf(stderr, "Read model with %d clusters took \t%lfs\n", model->size, ((double)(clock() - start)) / ((double)1000000));
}

Point *readExamples(int dimension, char *testName) {
    clock_t start = clock();
    char line[line_len + 1];
    //
    Point *exs;
    unsigned int exSize = 1;
    exs = malloc(exSize * sizeof(Point));
    //
    FILE *file;
    fprintf(stderr, "Reading test from     '%s'\n", testName);
    file = fopen(testName, "r");
    if (file == NULL) errx(EXIT_FAILURE, "bad file open '%s'", testName);
    // for (examples)
    while (fgets(line, line_len, file)) {
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
        if (assigned != 23) errx(EXIT_FAILURE, "File with wrong format  '%s'", testName);
        //
        exs = realloc(exs, (++exSize) * sizeof(Point));
    }
    fclose(file);
    Point *ex = &(exs[exSize-1]);
    ex->id = -1;
    ex->value = NULL;
    ex->label = '\0';
    fprintf(stderr, "Read test with %d examples took \t%lfs\n", exSize, ((double)(clock() - start)) / ((double)1000000));
    return exs;
}

void classify(int dimension, Model *model, Point *ex, Match *match) {
    match->distance = (double) dimension;
    match->pointId = ex->id;
    for (int i = 0; i < model->size; i++) {
        double distance = MNS_distance(ex->value, model->vals[i].center, dimension);
        if (match->distance > distance) {
            match->clusterId = model->vals[i].id;
            match->label = model->vals[i].label;
            match->radius = model->vals[i].radius;
            match->distance = distance;
        }
    }
    match->isMatch = match->distance <= match->radius ? 'y' : 'n';
    
    printf("%d,%c,%d,%c,%e,%e\n",
        match->pointId, match->isMatch, match->clusterId,
        match->label, match->distance, match->radius
    );
}

#endif // MINAS_FUNCS