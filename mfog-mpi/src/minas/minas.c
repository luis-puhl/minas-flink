#ifndef MINAS_C
#define MINAS_C

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <err.h>

#include "minas.h"
#include "../util/loadenv.h"

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
    // double *classifyDistances = malloc(model->size * sizeof(double));
    clock_t start = clock();
    //
    double globalDistance = dimension * 2.0, prevGlobalDistance, diffGD = 1.0;
    double newCenters[nClusters][dimension], distances[nClusters], sqrDistances[nClusters];
    int matches[nClusters];
    int maxIterations = 10;
    while (diffGD > 0.00001 && maxIterations-- > 0) {
        // setup
        prevGlobalDistance = globalDistance;
        globalDistance = 0.0;
        for (int i = 0; i < model->size; i++) {
            matches[model->vals[i].id] = 0;
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
            distances[match.clusterId] += match.distance;
            sqrDistances[match.clusterId] += match.distance * match.distance;
            for (int d = 0; d < dimension; d++) {
                newCenters[match.clusterId][d] += examples[i].value[d];
            }
            matches[match.clusterId]++;
        }
        // new centers and radius
        for (int i = 0; i < model->size; i++) {
            Cluster *cl = &(model->vals[i]);
            cl->matches = matches[cl->id];
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
        PRINT_TIMING(timing, executable, 1, start, nExamples);
    }
    return model;
}

Model* readModel(int dimension, FILE *file, FILE *timing, char *executable) {
    if (file == NULL) errx(EXIT_FAILURE, "bad file");
    clock_t start = clock();
    char line[line_len + 1];
    //
    Model *model = malloc(sizeof(Model));
    model->size = 0;
    model->dimension = dimension;
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
            "%d,%d,%lf,%lf,"
            "%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf\n",
            cl->id, cl->label, cl->category,
            cl->matches, cl->time, cl->meanDistance, cl->radius,
            cl->center[0], cl->center[1], cl->center[2], cl->center[3], cl->center[4],
            cl->center[5], cl->center[6], cl->center[7], cl->center[8], cl->center[9],
            cl->center[10], cl->center[11], cl->center[12], cl->center[13], cl->center[14],
            cl->center[15], cl->center[16], cl->center[17], cl->center[18], cl->center[19],
            cl->center[20], cl->center[21]
        );
    }
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
        if (match->distance > distance) {
            // match->cluster = &(model->vals[i]);
            match->clusterId = model->vals[i].id;
            match->clusterLabel = model->vals[i].label;
            match->clusterRadius = model->vals[i].radius;
            match->secondDistance = match->distance;
            match->distance = distance;
        }
    }
    // printf("\n");
    match->label = match->distance <= match->clusterRadius ? match->clusterLabel : '-';

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
Model* MNS_offline(int nExamples, Point examples[], int nClusters, int dimension, FILE *timing, char *executable) {
    // one kMeans per label
    clock_t start = clock();
    Model *model = malloc(sizeof(Model));
    model->size = 0;
    model->dimension = dimension;
    model->vals = malloc(1 * sizeof(Cluster));
    int filledClusters = 0;
    //
    int labelsSize = 1, groupSize = 0;
    char *labels = malloc(labelsSize * sizeof(char));
    labels[0] = examples[0].label;
    Point *group = malloc(nExamples * sizeof(Point));
    int remaining = nExamples;
    while (remaining > 0) {
        for (int i = 0; i < nExamples; i++) {
            if (labels[labelsSize - 1] == examples[i].label) {
                group[groupSize] = examples[i];
                groupSize++;
                remaining--;
            }
        }
        printf("remaining %d\n", remaining);

        // kMeansInit(model->size, model->dimension, group);
        model->size += nClusters;
        model->vals = realloc(model->vals, model->size * sizeof(Cluster));
        for (int i = filledClusters; i < model->size; i++) {
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
        if (timing) {
            PRINT_TIMING(timing, executable, 1, start, model->size);
        }

        filledClusters += nClusters;
        // next label
        if (remaining > 0) {
            groupSize = 0;
            for (int i = 0; i < nExamples; i++) {
                int j = 0;
                for (; j < labelsSize; j++) {
                    if (labels[j] == examples[i].label) break;
                }
                if (j == labelsSize) {
                    // ex.label not found in dictionary
                    labelsSize++;
                    labels = realloc(labels, labelsSize * sizeof(char));
                    labels[j] = examples[i].label;
                    break;
                }
            }
        }
    }
    return model;
}

#endif // MINAS_C
