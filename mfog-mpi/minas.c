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
    // printf("classify x_%d => %e, %e (%d, %x)\n", example->id, min, max, minCluster->id, minCluster);
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
        model->vals[i].radius = rand();
        model->vals[i].center = malloc(MNS_dimesion * sizeof(float));
        for (int j = 0; j < MNS_dimesion; j++) {
            model->vals[i].center[j] = rand();
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

int main(int argc, char const *argv[]) {
    srand(22);
    MNS_dimesion = 22;
    Model *model = initModel();
    printModel(model);
    //
    Point example;
    example.value = malloc(MNS_dimesion * sizeof(float));
    Match match;
    int matchesSize = 2;
    char *labels = malloc(matchesSize * sizeof(char));
    int *matches = malloc(matchesSize * sizeof(int));
    int noMatch = 0;
    for (example.id = 0; example.id < 653457; example.id++) {
        for (int i = 0; i < MNS_dimesion; i++) {
            example.value[i] = rand();
        }

        int hasMatch = classify(model, &example, &match);
        if (hasMatch) {
            int i;
            for (i = 0; i < matchesSize; i++) {
                if (labels[i] == '\0') {
                    printf("new label on match map -> %c\n", match.label);
                    labels[i] = match.label;
                    matches[i] = 1;
                    break;
                }
                if (match.label == labels[i]) {
                    matches[i]++;
                    hasMatch = 0;
                    break;
                }
            }
            if (hasMatch && labels[i] != '\0') {
                printf("reallocate match map\n");
                matchesSize *= 1.15;
                labels = realloc(labels, matchesSize * sizeof(char));
                matches = realloc(matches, matchesSize* sizeof(int));
            }
        } else {
            noMatch++;
        }
    }
    printf("label \tmatches\n");
    for (int i = 0; i < matchesSize; i++) {
        printf("%c \t%d\n", labels[i], matches[i]);
    }
    printf("None \t%d\n", noMatch);

    freeModel(model);
    free(labels);
    free(matches);
    exit(EXIT_SUCCESS);
}
