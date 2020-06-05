#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <err.h>
#include <string.h>

#include "./minas.h"
#include "./strsep.c"

extern int MNS_dimesion;

double MNS_distance(float a[], float b[]) {
    double distance = 0;
    for (int i = 0; i < MNS_dimesion; i++) {
        float diff = a[i] - b[i];
        distance += diff * diff;
    }
    return sqrt(distance);
}

int MNS_classify(Model* model, Point *example, Match *match) {
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
    match->isMatch = match->distance <= match->radius ? 'y' : 'n';
    return match->isMatch;
}

int MNS_printFloatArr(float* value) {
    if (value == NULL) return 0;
    int pr = 0;
    for (int i = 0; i < MNS_dimesion; i++) {
        pr += printf("%f, ", value[i]);
    }
    return pr;
}
int MNS_printPoint(Point *point) {
    if (point == NULL) return 0;
    int pr = printf("Point(id=%d, value=[", point->id);
    pr += MNS_printFloatArr(point->value);
    pr += printf("])\n");
    return pr;
}
int MNS_printCluster(Cluster *cl) {
    if (cl == NULL) return 0;
    int pr = printf("Cluster(id=%d, lbl=%c, tm=%d, r=%f, center=[", cl->id, cl->label, cl->time, cl->radius);
    pr += MNS_printFloatArr(cl->center);
    pr += printf("])\n");
    return pr;
}
int MNS_printModel(Model* model) {
    char *labels;
    labels = (char *) malloc(model->size * 3 * sizeof(char));
    for (int i = 0; i < model->size * 3; i += 3){
        labels[i] = model->vals[i].label;
        labels[i + 1] = ',';
        labels[i + 2] = ' ';
    }
    int pr = printf("Model(size=%d, labels=[%s])\n", model->size, labels);
    free(labels);
    for (int i = 0; i < model->size; i++){
        pr += MNS_printCluster(&(model->vals[i]));
    }
    return pr;
}

Model *MNS_readModelFile(const char *filename) {
    FILE *modelFile = fopen(filename, "r");
    if (modelFile == NULL) {
        errx(EXIT_FAILURE, "bad file open '%s'", filename);
    }
    Model *model = malloc(sizeof(Model));
    model->vals = malloc(1 * sizeof(Cluster));
    // char *separators = strdup("\n, []");
    #define line_len 10 * 1024
    char line[line_len];
    model->size = 0;
    while (fgets(line, line_len, modelFile)) {
        if (*line == '#') continue;
        model->vals = realloc(model->vals, (++model->size) * sizeof(Cluster));
        Cluster *cl = &(model->vals[model->size - 1]);
        if (cl->center == NULL) {
            cl->center = malloc(MNS_dimesion * sizeof(float));
        }
        int assigned = sscanf(line,
            "%d,%c,%c,"
            "%d,%d,%f,%f,"
            "%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f\n",
            &cl->id, &cl->label, &cl->category,
            &cl->matches, &cl->time, &cl->meanDistance, &cl->radius,
            &cl->center[0], &cl->center[1], &cl->center[2], &cl->center[3], &cl->center[4],
            &cl->center[5], &cl->center[6], &cl->center[7], &cl->center[8], &cl->center[9],
            &cl->center[10], &cl->center[11], &cl->center[12], &cl->center[13], &cl->center[14],
            &cl->center[15], &cl->center[16], &cl->center[17], &cl->center[18], &cl->center[19],
            &cl->center[20], &cl->center[21]
        );
        if (assigned != 29) {
            break;
        }
    }
    fclose(modelFile);
    return model;
}

/**
 * Fills an Point with the string format
 * 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,A
 */
int MNS_readExample(Point *ex, FILE *file) {
    #define line_len 10 * 1024
    char line[line_len];
    if (feof(file) || fgets(line, line_len, file) == 0) return 0;
    // while (line[0] == '#') if (!fgets(line, line_len, file)) return 0;
    int assigned = sscanf(line,
        "%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f"
        "%c\n",
        &ex->value[0], &ex->value[1], &ex->value[2], &ex->value[3], &ex->value[4],
        &ex->value[5], &ex->value[6], &ex->value[7], &ex->value[8], &ex->value[9],
        &ex->value[10], &ex->value[11], &ex->value[12], &ex->value[13], &ex->value[14],
        &ex->value[15], &ex->value[16], &ex->value[17], &ex->value[18], &ex->value[19],
        &ex->value[20], &ex->value[21], &ex->label
    );
    ex->id++;
    return !feof(file) && (assigned == 23);
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
    Model *model = MNS_readModelFile("datasets/model-clean.csv");
    fprintf(stderr, "Model read in \t%fs\n", ((double)(clock() - start)) / ((double)1000000));
    //
    Point example;
    example.value = malloc(MNS_dimesion * sizeof(float));
    example.id = 0;
    Match match;
    //
    FILE *kyotoOnl = fopen(argv[2], "r");
    if (kyotoOnl == NULL) errx(EXIT_FAILURE, "bad file open '%s'", argv[2]);
    // 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,A
    printf("id,isMach,clusterId,label,distance,radius\n");
    while (MNS_readExample(&example, kyotoOnl)) {
        MNS_classify(model, &example, &match);
        
        printf("%d,%c,%d,%c,%f,%f\n",
            match.pointId, match.isMatch, match.clusterId,
            match.label, match.distance, match.radius);
        example.id++;
    }
    fclose(kyotoOnl);
    // MNS_classifier(model, argv[2]);
    fprintf(stderr, "Total examples \t%d\n", example.id);

    for (int i = 0; i < model->size; i++) {
        free(model->vals[i].center);
    }
    free(model->vals);
    free(model);
    fprintf(stderr, "Done %s in \t%fs\n", argv[0], ((double)(clock() - start)) / ((double)1000000));
    exit(EXIT_SUCCESS);
}



/*
int MNS_classifier(Model *model, const char *filename) {
    Point example;
    example.id = 0;
    Match match;
    //
    FILE *kyotoOnl = fopen(filename, "r");
    if (kyotoOnl == NULL) errx(EXIT_FAILURE, "bad file open '%s'", filename);
    printf("id,isMach,clusterId,label,distance,radius\n");
    #define line_len 10 * 1024
    char line[line_len];
    char *separators = strdup(",");
    // while (!feof(kyotoOnl)) {
    //     example.id++;
    //     if (example.value == NULL) {
    //         example.value = malloc(MNS_dimesion * sizeof(float));
    //     }
    //     for (int i = 0; i < MNS_dimesion; i++) {
    //         fscanf(kyotoOnl, "%f,", &(example.value[i]));
    //     }
    //     fscanf(kyotoOnl, "%s\n", line);
    //     example.label = strdup(line);
    //     // printPoint(example);
    //     MNS_classify(model, &example, &match);
    //     printf("%d,%c,%d,%s,%f,%f\n",
    //         match.pointId, match.isMatch, match.clusterId,
    //         match.label, match.distance, match.radius);
    //     // printf("%d,%c,%d,%c,%f,%f\n", match.pointId, hasMatch ? 'y' : 'n', match.clusterId, match.label, match.distance, match.radius);
    // }
    Point *ex;
    ex = &example;
    char *sep;
    sep = separators;
    while (fgets(line, line_len, kyotoOnl)) {
        printf("'%s'\n", line);
        char *ptr, *token;
        token = strtok_r(line, sep, &ptr);
        if (!token) return 0;
        printf("line='%s'\n", line);
        ex->id++;
        if (ex->value == NULL) {
            ex->value = malloc(MNS_dimesion * sizeof(float));
        }
        for (int i = 0; i < MNS_dimesion; i++) {
            token = strtok_r(NULL, sep, &ptr);
            printf("token='%s'\n", token);
            if (!token) errx(EXIT_FAILURE, "Missing token in file '%s'", filename);
            ex->value[i] = atof(token);
        }
        ex->label = strdup(tokenOrFail(sep, &ptr, filename));
        if (!MNS_readExample(&line, &example, filename, separators)) {
            break;
        }
        MNS_classify(model, &example, &match);
        if (match.label == NULL) {
            errx(EXIT_FAILURE, "bad match label.\n");
        }
        printf("%d,%c,%d,%s,%f,%f\n",
            match.pointId, match.isMatch, match.clusterId,
            match.label, match.distance, match.radius);
    }
    fclose(kyotoOnl);
    return example.id;
}
*/
