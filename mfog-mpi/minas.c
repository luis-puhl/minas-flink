#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <err.h>

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
Model *readModel(const char* fileName) {
    FILE *modelFile = fopen(fileName, "r");
    if (modelFile == NULL) errx(EXIT_FAILURE, "bad file open '%s'", fileName);
    // id,label,category,matches,time,meanDistance,radius,center
    // 0,N,normal,502,0,0.04553028494064095,0.1736759823342961,[2.888834262948207E-4, 0.020268260292164667,0.04161011127902189, 0.020916334661354643, 1.0, 0.0, 0.0026693227091633474, 0.516593625498008, 0.5267529880478092, 1.9920318725099602E-4, 0.0, 7.968127490039841E-5, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0]
    char *header = malloc(1024 * sizeof(char));
    if (fscanf(modelFile, "%s\n", header)) {
        printf("csv header: %s\n", header);
        free(header);
    } else {
        errx(EXIT_FAILURE, "bad csv header '%s'\n", fileName);
    }

    #define PRINT_LABELS 1
    #ifdef PRINT_LABELS
    int labelsSize = 2;
    char *labels = malloc(labelsSize * sizeof(char));
    #endif
    Model *model = malloc(sizeof(Model));
    model->vals = malloc(1 * sizeof(Cluster));
    model->size = 1;
    //
    char category[1024];
    int id, matches, time, i;
    char label;
    float meanDistance, radius;
    float *center;
    while (!feof(modelFile)) {
        fscanf(modelFile, "%d,%c,", &id, &label);
        i = 0;
        fscanf(modelFile, "%c", &(category[i]));
        while (category[i] != ',') {
            fscanf(modelFile, "%c", &(category[++i]));
        }
        category[i] = '\0';
        fscanf(modelFile, "%d,%d,%f,%f,[", &matches, &time, &meanDistance, &radius);
        center = malloc(MNS_dimesion * sizeof(float));
        for (i = 0; i < (MNS_dimesion -1); i++) {
            fscanf(modelFile, "%f, ", &(center[i]));
        }
        fscanf(modelFile, "%f]\n", &(center[MNS_dimesion -1]));
        #ifdef PRINT_LABELS
        for (i = 0; i < labelsSize; i++) {
            if (labels[i] == '\0') {
                labels[i] = label;
                printf("%c ", labels[i]);
                labels = realloc(labels, (++labelsSize) * sizeof(Cluster));
                break;
            }
            if (labels[i] == label) break;
        }
        #endif

        i = model->size - 1;
        model->vals[i].id = i;
        model->vals[i].label = label;
        model->vals[i].lastTMS = clock();
        model->vals[i].radius = radius;
        model->vals[i].center = center;
        //
        if (!feof(modelFile)) {
            model->vals = realloc(model->vals, (++model->size) * sizeof(Cluster));
        }
    }
    #ifdef PRINT_LABELS
    printf("\n");
    #endif
    fclose(modelFile);
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

int next(Point* prev, FILE* file) {
    if (feof(file)) return 0;
    prev->id++;
    for (int i = 0; i < MNS_dimesion; i++) {
        fscanf(file, "%f,", &(prev->value[i]));
    }
    char l;
    fscanf(file, "%c\n", &l);
    return 1;
}

int main(int argc, char const *argv[]) {
    if (argc != 3)
        errx(EXIT_FAILURE, "Missing arguments, expected 2, got %d\n", argc - 1);
    printf("Reading model from %s\nand test from %s\n", argv[1], argv[2]);
    MNS_dimesion = 22;
    clock_t start = clock();
    srand(time(0));
    //
    Model *model = readModel(argv[1]);
    //
    Point example;
    example.value = malloc(MNS_dimesion * sizeof(float));
    Match match;
    int matchesSize = 2;
    char *labels = malloc(matchesSize * sizeof(char));
    int *matches = malloc(matchesSize * sizeof(int));
    int noMatch, hasMatch, matchIndex;
    //
    FILE *kyotoOnl = fopen(argv[2], "r");
    if (kyotoOnl == NULL) errx(EXIT_FAILURE, "bad file open '%s'", argv[2]);
    // 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,A
    while (next(&example, kyotoOnl)) {
        // printFloatArr(example.value);
        hasMatch = classify(model, &example, &match);
        if (!hasMatch) {
            noMatch++;
            continue;
        }
        for (matchIndex = 0; matchIndex < matchesSize; matchIndex++) {
            // printf("match -> %c == %c, %d, %d\n", match.label, labels[matchIndex], hasMatch);
            if (match.label == labels[matchIndex]) {
                matches[matchIndex]++;
                hasMatch = 0;
                break;
            }
            if (labels[matchIndex] == '\0') {
                printf("new label on match map -> %c\n", match.label);
                labels[matchIndex] = match.label;
                matches[matchIndex] = 1;
                if (matchIndex != 0) {
                    matchesSize++;
                    labels = realloc(labels, matchesSize * sizeof(char));
                    matches = realloc(matches, matchesSize * sizeof(int));
                }
                break;
            }
        }
    }
    fclose(kyotoOnl);
    //
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
