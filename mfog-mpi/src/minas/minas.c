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
    if (timing) {
        PRINT_TIMING(timing, executable, 1, start, model->size);
    }
    return model;
}

void writeModel(int dimension, FILE *file, Model *model, FILE *timing, char *executable) {
    if (file == NULL) errx(EXIT_FAILURE, "bad file");
    clock_t start = clock();
    //
    // #id,label,category,matches,time,meanDistance,radius,c0,c1,c2,c3,c4,c5,c6,c7,c8,c9,c10,c11,c12,c13,c14,c15,c16,c17,c18,c19,c20,c21
    fprintf(file,
        "#id,label,category,matches,time,meanDistance,radius,"
        "c0,c1,c2,c3,c4,c5,c6,c7,c8,c9,c10,c11,c12,c13,c14,c15,c16,c17,c18,c19,c20,c21\n"
    );
    for (int i = 0; i < model->size; i++) {
        Cluster *cl = &(model->vals[i]);
        fprintf(file,
            "%d,%c,%c,"
            "%d,%d,%le,%le,"
            "%le,%le,%le,%le,%le,%le,%le,%le,%le,%le,%le,%le,%le,%le,%le,%le,%le,%le,%le,%le,%le,%le\n",
            cl->id, cl->label, cl->category,
            cl->matches, cl->time, cl->meanDistance, cl->radius,
            cl->center[0], cl->center[1], cl->center[2], cl->center[3], cl->center[4],
            cl->center[5], cl->center[6], cl->center[7], cl->center[8], cl->center[9],
            cl->center[10], cl->center[11], cl->center[12], cl->center[13], cl->center[14],
            cl->center[15], cl->center[16], cl->center[17], cl->center[18], cl->center[19],
            cl->center[20], cl->center[21]
        );
    }
    fflush(file);
    if (timing) {
        PRINT_TIMING(timing, executable, 1, start, model->size);
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
        ex->id = exSize - 1;
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
        PRINT_TIMING(timing, executable, 1, start, (*nExamples));
    }
    return exs;
}

void classify(int dimension, Model *model, Point *ex, Match *match) {
    match->distance = (double) dimension;
    match->pointId = ex->id;
    // printf("#pid_%d", ex->id);
    for (int i = 0; i < model->size; i++) {
        double distance = MNS_distance(ex->value, model->vals[i].center, dimension);
        // printf("%le,", distance);
        // allDistances[i] = distance;
        if (match->distance >= distance) {
            // match->cluster = &(model->vals[i]);
            match->clusterId = model->vals[i].id;
            match->clusterLabel = model->vals[i].label;
            match->clusterCatergoy = model->vals[i].category;
            match->clusterRadius = model->vals[i].radius;
            match->secondDistance = match->distance;
            match->distance = distance;
        }
    }
    // printf("\n");
    match->label = match->distance < match->clusterRadius ? match->clusterLabel : '-';

    // printf("%d,%c,%d,%c,%e,%e\n",
    //     match->pointId, match->isMatch, match->clusterId,
    //     match->label, match->distance, match->radius
    // );
}

int MNS_minas_main(int argc, char *argv[], char **envp) {
    char *executable = argv[0];
    char *modelCsv, *examplesCsv, *matchesCsv, *timingLog;
    FILE *modelFile, *examplesFile, *matches, *timing;
    #define VARS_SIZE 4
    char *varNames[] = { "MODEL_CSV", "EXAMPLES_CSV", "MATCHES_CSV", "TIMING_LOG"};
    char **fileNames[] = { &modelCsv, &examplesCsv, &matchesCsv, &timingLog };
    FILE **files[] = { &modelFile, &examplesFile, &matches, &timing };
    char *fileModes[] = { "r", "r", "w", "a" };
    loadEnv(argc, argv, envp, VARS_SIZE, varNames, fileNames, files, fileModes);
    printf(
        "Reading model from     '%s'\n"
        "Reading examples from  '%s'\n"
        "Writing matches to     '%s'\n"
        "Writing timing to      '%s'\n",
        modelCsv, examplesCsv, matchesCsv, timingLog
    );
    //
    
    Model *model = readModel(22, modelFile, timing, executable);
    Point *examples;
    int nExamples;
    examples = readExamples(model->dimension, examplesFile, &nExamples, timing, executable);
    // fprintf(stderr, "nExamples %d\n", nExamples);

    fprintf(matches, MATCH_CSV_HEADER);
    int exampleCounter = 0;
    clock_t start = clock();
    Match match;
    Point *unkBuffer = malloc(nExamples * sizeof(Point));
    int nUnk = 0;
    for (exampleCounter = 0; exampleCounter < nExamples; exampleCounter++) {
        // fprintf(stderr, "%d/%d\n", exampleCounter, nExamples);
        //
        classify(model->dimension, model, &(examples[exampleCounter]), &match);
        if (match.label == '-') {
            // unkown
            unkBuffer[nUnk] = examples[exampleCounter];
            nUnk++;
        }
        if (nUnk > 100 && nUnk % 100 == 0) {
            // retrain
            Model m;
            m.dimension = 22;
            m.size = 10;
            m.vals = malloc(m.size * sizeof(Cluster));
            for (int i = 0; i < m.size; i++) {
                m.vals[i].id = i;
                m.vals[i].center = malloc(m.dimension * sizeof(double));
            }
            // kMeans(&m, m.size, dimension, unkBuffer, nUnk, timing, executable);
        }
        fprintf(matches, MATCH_CSV_LINE_FORMAT, MATCH_CSV_LINE_PRINT_ARGS(match));
    }
    PRINT_TIMING(timing, executable, 1, start, exampleCounter);

    closeEnv(VARS_SIZE, varNames, fileNames, files, fileModes);

    for (int i = 0; i < model->size; i++) {
        free(model->vals[i].center);
    }
    free(model->vals);
    free(examples);
    return 0;
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
            errx(EXIT_FAILURE, "Not enough examples for clustering. Needed %d and got %d\n", k, groupSize);
        }
        //
        int prevModelSize = model->size;
        model->size += k;
        printf("realloc from %d to %d\n", prevModelSize, model->size);
        model->vals = realloc(model->vals, model->size * sizeof(Cluster));
        //
        Cluster *clusters = &(model->vals[prevModelSize]);
        clusters = kMeansInit(k, clusters, dimension, groupSize, linearGroup, prevModelSize, label, category, timing, executable);
        //
        clusters = kMeans(k, clusters, dimension, groupSize, linearGroup, timing, executable);
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

char noveltyLabel(Model *model, Cluster *cluster, double threshold) {
    double minDist = model->dimension;
    Cluster *nearest = NULL;
    for (int i = 0; i < model->size; i++) {
        double dist = MNS_distance(model->vals[i].center, cluster->center, model->dimension);
        if (dist < minDist || nearest == NULL) {
            nearest = &(model->vals[i]);
            minDist = dist;
        }
    }
    if (minDist < nearest->meanDistance * threshold) {
        // cluster in an extension of a concept
        cluster->label = nearest->label;
    } else {
        // cluster is a novelty pattern
        printf("Novelty pattern found. Will call it '%c'.\n", model->nextNovelty);
        cluster->label = model->nextNovelty;
        model->nextNovelty++;
        if (model->nextNovelty > 'z') {
            model->nextNovelty = 'a';
        }
    }
    return cluster->label;
}

Model *noveltyDetection(int k, Model *model, int unknownsSize, Point unknowns[], int minExCluster, double noveltyThreshold, FILE *timing, char *executable) {
    clock_t start = clock();
    char label = 'a';
    char category = 'u';
    int dimension = model->dimension;
    Cluster *clusters = malloc(k * sizeof(Cluster));

    clusters = kMeansInit(k, clusters, dimension, unknownsSize, unknowns, model->size, label, category, timing, executable);
    //
    clusters = kMeans(k, clusters, dimension, unknownsSize, unknowns, timing, executable);

    for (int clId = 0; clId < k; clId++) {
        if (clusters[clId].matches > minExCluster) {
            noveltyLabel(model, &clusters[clId], noveltyThreshold);
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
