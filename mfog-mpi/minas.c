#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <err.h>
#include <string.h>

#include "minas.h"
#include "strsep.c"

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
    // since most problems are in the range [0,1], max distance is sqrt(dimesion)
    match->distance = (float) MNS_dimesion;
    match->pointId = example->id;
    for (int i = 0; i < model->size; i++) {
        float distance = MNS_distance(model->vals[i].center, example->value);
        if (match->distance > distance) {
            match->clusterId = model->vals[i].id;
            match->label = model->vals[i].label;
            match->radius = model->vals[i].radius;
            match->distance = distance;
        }
    }
    /*
    printf(
        "classify x_%d [0]=%f => min=%e, max=%e, (c0=%e, c=%d, r=%e, m=%d, l=%c)\n",
        example->id, example->value[0], min, max, minCluster->center[0], minCluster->id,
        minCluster->radius, min <= minCluster->radius, minCluster->label
    );
    */
   return match->distance <= match->radius;
}

int printFloatArr(float* value) {
    if (value == NULL) return 0;
    int pr = 0;
    for (int i = 0; i < MNS_dimesion; i++) {
        pr += printf("%f, ", value[i]);
    }
    return pr;
}
int printPoint(Point *point) {
    if (point == NULL) return 0;
    int pr = printf("Point(id=%d, value=[", point->id);
    pr += printFloatArr(point->value);
    pr += printf("])\n");
    return pr;
}
int printCluster(Cluster *cl) {
    if (cl == NULL) return 0;
    int pr = printf("Cluster(id=%d, lbl=%c, tm=%ld, r=%f, center=[", cl->id, cl->label, cl->lastTMS, cl->radius);
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
    if (argc != 3) {
        errx(EXIT_FAILURE, "Missing arguments, expected 2, got %d\n", argc - 1);
    }
    fprintf(stderr, "Reading model from \t'%s'\nReading test from \t'%s'\n", argv[1], argv[2]);
    MNS_dimesion = 22;
    clock_t start = clock();
    srand(time(0));
    //
    FILE *modelFile = fopen(argv[1], "r");
    if (modelFile == NULL) {
        errx(EXIT_FAILURE, "bad file open '%s'", argv[1]);
    }
    // id,label,category,matches,time,meanDistance,radius,center
    // 0,N,normal,1,0,0.0,0.1,[2.8E-4, 0.02, 0.0, 0.0, 1.0, 0.0, 0.0, 0.5, 0.5, 1.9E-4, 0.0, 7.9E-5, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0]
    Model *model = malloc(sizeof(Model));
    model->size = 1;
    model->vals = malloc(model->size * sizeof(Cluster));
    //
    Cluster *cl;
    int i, matches;
    float meanDistance;
    //
    cl = &(model->vals[model->size - 1]);
    char *searators = strdup("\n, []");
    char *ptr;
    //
    #define line_len 10 * 1024
    char line[line_len];
    fgets(line, line_len, modelFile);
    // printf("%s", line);
    while (fgets(line, line_len, modelFile)) {
        // printf("'%s'\n", line);
        cl->id = atoi(strtok_r(line, searators, &ptr));
        char *label = strtok_r(NULL, searators, &ptr);
        cl->label = label[0];
        char *category = strtok_r(NULL, searators, &ptr);
        matches = atoi(strtok_r(NULL, searators, &ptr));
        cl->lastTMS = atoi(strtok_r(NULL, searators, &ptr));
        meanDistance = atof(strtok_r(NULL, searators, &ptr));
        cl->radius = atof(strtok_r(NULL, searators, &ptr));
        cl->radius = 22.0;
        cl->center = malloc(MNS_dimesion * sizeof(float));
        for (i = 0; i < MNS_dimesion; i++) {
            cl->center[i] = atof(strtok_r(NULL, searators, &ptr));
        }
        // printCluster(cl);
        // printf("%d,%c,%s,%d,%ld,%f,%f,[", cl->id, cl->label, category, matches, cl->lastTMS, meanDistance, cl->radius);
        // for (i = 0; i < MNS_dimesion; i++) printf("%f, ", cl->center[i]);
        // printf("]\n");
    }
    fprintf(stderr, "Model read in \t%fs\n", ((double)(clock() - start)) / ((double)1000000));
    fclose(modelFile);
    // printModel(model);
    // exit(EXIT_SUCCESS);
    //
    Point example;
    example.value = malloc(MNS_dimesion * sizeof(float));
    example.id = 0;
    Match match;
    int hasMatch;
    //
    FILE *kyotoOnl = fopen(argv[2], "r");
    if (kyotoOnl == NULL) errx(EXIT_FAILURE, "bad file open '%s'", argv[2]);
    // 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,A
    char l;
    printf("id,isMach,clusterId,label,distance,radius\n");
    while (!feof(kyotoOnl)) {
        for (int i = 0; i < MNS_dimesion; i++) {
            fscanf(kyotoOnl, "%f,", &(example.value[i]));
        }
        fscanf(kyotoOnl, "%c\n", &l);
        // printPoint(example);
        hasMatch = classify(model, &example, &match);
        if (match.label == '\0') {
            errx(EXIT_FAILURE, "bad match label '%c'\n", match.label);
        }
        printf("%d,%c,%d,%c,%f,%f\n", match.pointId, hasMatch ? 'y' : 'n', match.clusterId, match.label, match.distance, match.radius);
        example.id++;
    }
    fclose(kyotoOnl);
    //
    fprintf(stderr, "Total examples \t%d\n", example.id);

    for (int i = 0; i < model->size; i++) {
        free(model->vals[i].center);
    }
    free(model->vals);
    free(model);
    fprintf(stderr, "Done %s in \t%fs\n", argv[0], ((double)(clock() - start)) / ((double)1000000));
    exit(EXIT_SUCCESS);
}
