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

Cluster* kMeansInit(int nClusters, int dimension, Point examples[], int initialClusterId, char label, char category, FILE *timing, char *executable) {
    clock_t start = clock();
    Cluster *clusters = malloc(nClusters * sizeof(Cluster));
    for (int i = 0; i < nClusters; i++) {
        clusters[i].id = initialClusterId + i;
        clusters[i].center = malloc(dimension * sizeof(double));
        clusters[i].pointSum = malloc(dimension * sizeof(double));
        clusters[i].pointSqrSum = malloc(dimension * sizeof(double));
        for (int j = 0; j < dimension; j++) {
            clusters[i].center[j] = examples[i].value[j];
            clusters[i].pointSum[j] = 0.0;
            clusters[i].pointSqrSum[j] = 0.0;
        }
        clusters[i].label = label;
        clusters[i].category = category;
        clusters[i].time = 0;
        clusters[i].matches = 0;
        clusters[i].meanDistance = 0.0;
        clusters[i].radius = 0.0;
    }
    if (timing) {
        PRINT_TIMING(timing, executable, 1, start, nClusters);
    }
    return clusters;
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

// void printModel(Model *model, Cluster clusters[], int nClusters) {
//     printf("Model(dim=%d, size=%d, vals=%p\n", model->dimension, model->size, model->vals);
//     for (int i = 0; i < nClusters; i++) {
//         Cluster *cl = &(clusters[i]);
//         double sum = 0.0;
//         for (int d = 0; d < model->dimension; d++) {
//             sum += cl->center[d];
//         }
//         printf(
//             "\tCluster(id=%2d, lbl=%c, cat=%c, "
//             "matches=%5d, tim=%d, mean=%le, radi=%le, val=%le\n",
//             cl->id, cl->label, cl->category,
//             cl->matches, cl->time, cl->meanDistance, cl->radius, sum);
//     }
// }

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
    for (int i = 0; i < nExamples; i++) {
        group[i].value = malloc(dimension * sizeof(double));
    }
    Match *groupMatches = malloc(nExamples * sizeof(Match));
    Cluster *clusters;
    int remaining = nExamples;
    while (remaining > 0) {
        char label = labels[labelsSize - 1];
        char category = 'n';
        for (int i = 0; i < nExamples; i++) {
            if (label == examples[i].label) {
                group[groupSize] = examples[i];
                groupSize++;
                remaining--;
            }
        }
        //
        printf("clustering %d examples with label %c\n", groupSize, label);
        //
        // Cluster *clusters = kMeansInit(nClusters, dimension, examples, initialClusterId, label, 'n', timing, executable);
        printf("realloc from %d to %d\n", model->size, model->size + nClusters);
        model->vals = realloc(model->vals, (model->size + nClusters) * sizeof(Cluster));
        clusters = &(model->vals[model->size]);
        for (int i = 0; i < nClusters; i++) {
            Cluster *modelCl = &(model->vals[model->size + i]);
            modelCl->id = model->size + i;
            modelCl->label = label;
            modelCl->category = category;
            modelCl->time = 0;
            modelCl->matches = 0;
            modelCl->meanDistance = 0.0;
            modelCl->radius = 0.0;
            modelCl->center = malloc(dimension * sizeof(double));
            modelCl->pointSum = malloc(dimension * sizeof(double));
            modelCl->pointSqrSum = malloc(dimension * sizeof(double));
            for (int d = 0; d < dimension; d++) {
                modelCl->center[d] = group[i].value[d];
                modelCl->pointSum[d] = 0.0;
                modelCl->pointSqrSum[d] = 0.0;
            }
        }
        model->size += nClusters;
        // printModel(model, model->vals, model->size);
        //
        // kMeans(clusters, groupMatches, nClusters, dimension, examples, nExamples, timing, executable);
        double globalInnerDistance = dimension;
        double prevGlobalInnerDistance = dimension + 1;
        double improvement = (globalInnerDistance / prevGlobalInnerDistance) - 1;
        improvement = improvement > 0 ? improvement : -improvement;
        for (int iter = 0; iter < 10 && improvement > (1.0E-05); iter++) {
            prevGlobalInnerDistance = globalInnerDistance;
            globalInnerDistance = 0;
            for (int i = 0; i < nClusters; i++) {
                clusters[i].matches = 0;
            }
            // distances
            for (int exIndx = 0; exIndx < nExamples; exIndx++) {
                // classify(dimension, model, &(group[i]), &m);
                Point *ex = &(group[exIndx]);
                Match *match = &(groupMatches[exIndx]);
                // printf("classify %d %d\n", exIndx, group[exIndx].id);
                match->distance = (double) dimension;
                match->pointId = ex->id;
                for (int clIndx = 0; clIndx < nClusters; clIndx++) {
                    Cluster *cl = &(clusters[clIndx]);
                    double distance = MNS_distance(ex->value, cl->center, dimension);
                    if (match->distance > distance) {
                        match->cluster = cl;
                        // match->clusterId = cl->id;
                        // match->clusterLabel = cl->label;
                        // match->clusterRadius = cl->radius;
                        match->secondDistance = match->distance;
                        match->distance = distance;
                    } else if (distance <= match->secondDistance) {
                        match->secondDistance = distance;
                    }
                }
                match->label = match->distance <= match->clusterRadius ? match->clusterLabel : '-';
                // update cluster
                // printf("update cluster %d -> %p\n", match->clusterId, match->cluster);
                match->cluster->matches++;
                match->cluster->distancesSum += match->distance;
                match->cluster->distancesSqrSum += match->distance * match->distance;
                for (int d = 0; d < dimension; d++) {
                    match->cluster->pointSum[d] += ex->value[d];
                    match->cluster->pointSqrSum[d] += ex->value[d] * ex->value[d];
                }
            }
            // assing new center
            for (int clIdx = 0; clIdx < nClusters; clIdx++) {
                Cluster *cl = &clusters[clIdx];
                if (cl->matches == 0) continue;
                // printf("assing new center to %d (%le avg dist)\n", cl->id, cl->distancesSum / cl->matches);
                for (int d = 0; d < dimension; d++) {
                    cl->center[d] = cl->pointSum[d] / cl->matches;
                    cl->pointSum[d] = 0;
                    cl->pointSqrSum[d] = 0;
                }
                cl->distancesSum = 0;
            }
            // update distances
            for (int i = 0; i < nExamples; i++) {
                groupMatches[i].distance = MNS_distance(group[i].value, groupMatches[i].cluster->center, dimension);
                groupMatches[i].cluster->distancesSum += groupMatches[i].distance;
                groupMatches[i].cluster->distancesSqrSum += groupMatches[i].distance * groupMatches[i].distance;
                globalInnerDistance += groupMatches[i].distance;
            }
            // update mean
            for (int clIdx = 0; clIdx < nClusters; clIdx++) {
                Cluster *cl = &clusters[clIdx];
                if (cl->matches != 0) {
                    cl->meanDistance = cl->distancesSum / cl->matches;
                }
                cl->radius = 0;
            }
            // update std-dev
            for (int i = 0; i < nExamples; i++) {
                double diff = groupMatches[i].distance - groupMatches[i].cluster->meanDistance;
                groupMatches[i].cluster->radius += diff * diff;
            }
            for (int clIdx = 0; clIdx < nClusters; clIdx++) {
                clusters[clIdx].radius = sqrt(clusters[clIdx].radius / (clusters[clIdx].matches - 1));
                // clusters[clIdx].matches = 0;
            }
            // stop when iteration limit is reached or when improvement is less than 1%
            improvement = (globalInnerDistance / prevGlobalInnerDistance) - 1;
            improvement = improvement > 0 ? improvement : - improvement;
            printf(
                "[%d] Global dist of %le (%le avg) (%10lf%% better)\n",
                iter, globalInnerDistance, globalInnerDistance / nExamples, improvement
            );
        // } while (iter > 0 && improvement > 1.01);
        }
        //
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
    // be free
    free(labels);

    for (int i = 0; i < nExamples; i++) {
        free(group[i].value);
    }
    free(group);

    free(groupMatches);
    //
    // dont free(model)
    // dont free(vals)
    // printf("%d %s\n", __LINE__, __FUNCTION__);
    // printModel(model, model->vals, model->size);
    return model;
}

#endif // MINAS_C
