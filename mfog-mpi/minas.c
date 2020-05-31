#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#include "minas.h"

extern int MNS_dimesion;

double MNS_distance(float a[], float b[]) {
    double distance = 0;
    for (int i = 0; i < MNS_dimesion; i++) {
        float diff = a[i] - b[i];
        distance += diff * diff;
    }
    return sqrt(distance);
}

int classify(Model* model, Point *example, Match *match) {
    Cluster* minCluster = NULL;
    float min, max;
    for (int i = 0; i < model->size; i++) {
        float distance = MNS_distance(model->vals[i].center, example->value);
        if (minCluster == NULL || min > distance) {
            minCluster = &(model->vals[i]);
            min = distance;
        }
        max = distance > max ? distance : max;
    }
    /*
    printf(
        "classify x_%d [0]=%f => min=%e, max=%e, (c0=%e, c=%d, r=%e, m=%d, l=%c)\n",
        example->id, example->value[0], min, max, minCluster->center[0], minCluster->id,
        minCluster->radius, min <= minCluster->radius, minCluster->label
    );
    */
    if (min <= minCluster->radius) {
        match->clusterId = minCluster->id;
        match->pointId = example->id;
        match->distance = min;
        match->label = minCluster->label;
        return 1;
    }
    return 0;
}

Model *initModel() {
    Model * model = malloc(sizeof(Model));
    model->size = 100;
    model->vals = malloc(model->size * sizeof(Cluster));
    for (int i = 0; i < model->size; i++) {
        model->vals[i].id = i;
        model->vals[i].label = (i % ('z' - 'a')) + 'a';
        model->vals[i].lastTMS = clock();
        model->vals[i].radius = 1.0 / ((float) MNS_dimesion);
        model->vals[i].center = malloc(MNS_dimesion * sizeof(float));
        for (int j = 0; j < MNS_dimesion; j++) {
            model->vals[i].center[j] = ((float) i) / ((float) model->size);
        }
    }
    return model;
}
void freeModel(Model* model) {
    for (int i = 0; i < model->size; i++) {
        free(model->vals[i].center);
    }
    free(model->vals);
    free(model);
}

int printFloatArr(float* value) {
    if (value == NULL) return 0;
    int pr = 0;
    for (int i = 0; i < MNS_dimesion; i++) {
        pr += printf("%f, ", value[i]);
    }
    return pr;
}
int rintPoint(Point *point) {
    if (point == NULL) return 0;
    int pr = printf("Point(id=%d, value=[", point->id);
    pr += printFloatArr(point->value);
    pr += printf("])\n");
    return pr;
}
int printCluster(Cluster *cl) {
    if (cl == NULL) return 0;
    int pr = printf("Cluster(id=%d, lbl=%c, tm=%ld, r=%f, value=[", cl->id, cl->label, cl->lastTMS, cl->radius);
    pr += printFloatArr(cl->center);
    pr += printf("])\n");
    return pr;
}
int printModel(Model* model){
    char *labels = malloc(model->size * 3 * sizeof(char));
    for (int i = 0; i < model->size * 3; i += 3){
        labels[i] = model->vals[i].label;
        labels[i + 1] = ',';
        labels[i + 2] = ' ';
    }
    int pr = printf("Model(size=%d, labels=[%s])\n", model->size, labels);
    free(labels);
    for (int i = 0; i < model->size; i++){
        pr += printCluster(&(model->vals[i]));
    }
    return pr;
}

int next(Point* prev, int max) {
    prev->id++;
    for (int i = 0; i < MNS_dimesion; i++) {
        prev->value[i] = ((float) prev->id) / ((float) max);
    }
    return prev->id < max;
}

int main(int argc, char const *argv[]) {
    clock_t start = clock();
    srand(time(0));
    MNS_dimesion = 22;
    Model *model = initModel();
    // printModel(model);
    //
    Point example;
    example.value = malloc(MNS_dimesion * sizeof(float));
    Match match;
    int matchesSize = 2;
    char *labels = malloc(matchesSize * sizeof(char));
    int *matches = malloc(matchesSize * sizeof(int));
    int noMatch, hasMatch, matchIndex;
    while (next(&example, 653457)) {
        hasMatch = classify(model, &example, &match);
        if (!hasMatch) {
            noMatch++;
            continue;
        }
        for (matchIndex = 0; matchIndex < matchesSize; matchIndex++) {
            // printf("match -> %c == %c, %d, %d\n", match.label, labels[i], hasMatch);
            if (match.label == labels[matchIndex]) {
                matches[matchIndex]++;
                hasMatch = 0;
                break;
            }
            if (labels[matchIndex] == '\0') {
                // printf("new label on match map -> %c\n", match.label);
                labels[matchIndex] = match.label;
                matches[matchIndex] = 1;
                break;
            }
        }
        if (matchIndex >= matchesSize) {
            matchesSize += 3;
            // printf("reallocate match map %d\n", matchesSize);
            labels = realloc(labels, matchesSize * sizeof(char));
            matches = realloc(matches, matchesSize* sizeof(int));
        }
    }
    int total = 0;
    printf("label \tmatches\n");
    for (int i = 0; i < matchesSize && labels[i] != 0; i++) {
        printf("%c %d \t%d\n", labels[i], labels[i], matches[i]);
        total += matches[i];
    }
    printf("None \t%d\n", noMatch);
    total += noMatch;
    printf("Total: \t%d\n", total);

    freeModel(model);
    free(labels);
    free(matches);
    clock_t diff = clock() - start;
    printf("Done in \t%fs\n", ((double) diff) / ((double) 1000000));
    exit(EXIT_SUCCESS);
}
