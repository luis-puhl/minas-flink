#ifndef _BASE_C
#define _BASE_C 1

#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
// #include <unistd.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>

#include "./base.h"

char *printableLabelReuse(unsigned int label, char *ret) {
    if (isalpha(label) || label == '-') {
        ret[0] = label;
        ret[1] = '\0';
    } else {
        sprintf(ret, "%u", label);
    }
    return ret;
}
/**
 * DON'T FORGET TO FREE
 */
char *printableLabel(unsigned int label) {
    return printableLabelReuse(label, calloc(20, sizeof(char)));
}

unsigned int fromPrintableLabel(char *label) {
    unsigned int ret;
    if (isalpha(label[0]) || label[0] == '-') {
        ret = label[0];
    } else {
        int l;
        sscanf(label, "%4u", &l);
        ret = l;
    }
    return ret;
}

double nearestClusterVal(int dim, Cluster clusters[], unsigned int nClusters, double val[], Cluster **nearest, char skipSleep) {
    double minDist;
    *nearest = NULL;
    for (unsigned int k = 0; k < nClusters; k++) {
        if (skipSleep && clusters[k].isSleep) {
            continue;
        }
        double dist = 0.0;
        for (unsigned int d = 0; d < dim; d++) {
            double v = (clusters[k].center[d] - val[d]);
            // dist += fabs(v);
            dist += v * v;
            if (k > 0 && dist > minDist) break;
        }
        if (k == 0 || dist <= minDist) {
            minDist = dist;
            *nearest = &clusters[k];
        }
    }
    return sqrt(minDist);
}

Cluster* kMeansInit(int kParam, int dim, Example trainingSet[], unsigned int trainingSetSize, unsigned int initalId) {
    if (trainingSetSize < kParam) {
        errx(EXIT_FAILURE, "Not enough examples for K-means. At "__FILE__":%d\n", __LINE__);
    }
    Cluster empty = CLUSTER_EMPTY;
    Cluster *clusters = calloc(kParam, sizeof(Cluster));
    for (size_t i = 0; i < kParam; i++) {
        clusters[i] = empty;
        clusters[i].id = initalId + i;
        clusters[i].center = calloc(dim, sizeof(double));
        clusters[i].ls_valLinearSum = calloc(dim, sizeof(double));
        clusters[i].ss_valSquareSum = calloc(dim, sizeof(double));
        for (size_t d = 0; d < dim; d++) {
            clusters[i].center[d] = trainingSet[i].val[d];
            clusters[i].ls_valLinearSum[d] = trainingSet[i].val[d];
            clusters[i].ss_valSquareSum[d] = trainingSet[i].val[d] * trainingSet[i].val[d];
        }
    }
    return clusters;
}

double kMeans(int kParam, int dim, double precision, Cluster clusters[], Example trainingSet[], unsigned int trainingSetSize) {
    double improvement, prevGlobalDistance, globalDistance = dim * kParam * trainingSetSize * 2;
    unsigned int iteration = 0;
    do {
        prevGlobalDistance = globalDistance;
        globalDistance = 0.0;
        for (unsigned int i = 0; i < trainingSetSize; i++) {
            Cluster *nearest = NULL;
            double minDist = nearestClusterVal(dim, clusters, kParam, trainingSet[i].val, &nearest, 0);
            globalDistance += minDist;
            nearest->n_matches++;
            for (unsigned int d = 0; d < dim; d++) {
                nearest->ls_valLinearSum[d] += trainingSet[i].val[d];
                nearest->ss_valSquareSum[d] += trainingSet[i].val[d] * trainingSet[i].val[d];
            }
        }
        for (unsigned int k = 0; k < kParam; k++) {
            for (size_t d = 0; d < dim; d++) {
                if (clusters[k].n_matches > 0)
                    clusters[k].center[d] = clusters[k].ls_valLinearSum[d] / clusters[k].n_matches;
                clusters[k].ls_valLinearSum[d] = 0.0;
                clusters[k].ss_valSquareSum[d] = 0.0;
            }
            clusters[k].n_matches = 0;
        }
        improvement = globalDistance - prevGlobalDistance;
        // fprintf(stderr, "\t[%3u] k-Means %le -> %le (%+le / %+le)\n", iteration, prevGlobalDistance, globalDistance, improvement, precision);
        iteration++;
    } while (fabs(improvement) > precision && iteration < 100);
    return globalDistance;
}

// Cluster cluStream() {
    
// }

Cluster* clustering(MinasParams *params, Example trainingSet[], unsigned int trainingSetSize, unsigned int initalId) {
    Cluster *clusters = kMeansInit(params->k, params->dim, trainingSet, trainingSetSize, initalId);
    kMeans(params->k, params->dim, params->precision, clusters, trainingSet, trainingSetSize);
    //
    double *distances = calloc(trainingSetSize, sizeof(double));
    Cluster **n_matches = calloc(trainingSetSize, sizeof(Cluster *));
    for (unsigned int k = 0; k < params->k; k++) {
        clusters[k].radius = 0.0;
        clusters[k].n_matches = 0;
        clusters[k].distanceMax = 0.0;
        clusters[k].distanceLinearSum = 0.0;
        clusters[k].distanceSquareSum = 0.0;
    }
    for (unsigned int i = 0; i < trainingSetSize; i++) {
        Cluster *nearest = NULL;
        double minDist = nearestClusterVal(params->dim, clusters, params->k, trainingSet[i].val, &nearest, 0);
        distances[i] = minDist;
        n_matches[i] = nearest;
        //
        nearest->n_matches++;
        nearest->distanceLinearSum += minDist;
        nearest->distanceSquareSum += minDist * minDist;
        if (minDist > nearest->distanceMax) {
            nearest->distanceMax = minDist;
        }
        for (unsigned int d = 0; d < params->dim; d++) {
            nearest->ls_valLinearSum[d] += trainingSet[i].val[d];
            nearest->ss_valSquareSum[d] += trainingSet[i].val[d] * trainingSet[i].val[d];
        }
    }
    for (unsigned int k = 0; k < params->k; k++) {
        if (clusters[k].n_matches == 0) continue;
        clusters[k].distanceAvg = clusters[k].distanceLinearSum / clusters[k].n_matches;
        clusters[k].distanceStdDev = 0.0;
        for (size_t i = 0; i < trainingSetSize; i++) {
            if (n_matches[i] == &clusters[k]) {
                double p = distances[i] - clusters[k].distanceAvg;
                clusters[k].distanceStdDev += p * p;
            }
        }
        clusters[k].distanceStdDev = sqrt(clusters[k].distanceStdDev);
        clusters[k].radius = params->radiusF * clusters[k].distanceStdDev;
        free(clusters[k].ls_valLinearSum);
        free(clusters[k].ss_valSquareSum);
    }
    free(distances);
    free(n_matches);
    return clusters;
}

Model *training(MinasParams *params) {
    unsigned int id = 0, nClasses = 0;
    Example *trainingSetByClass[255];
    char classes[255];
    unsigned int classesSize[255];
    for (size_t l = 0; l < 255; l++) {
        trainingSetByClass[l] = calloc(1, sizeof(Example));
        classesSize[l] = 0;
        classes[l] = '\0';
    }
    fprintf(stderr, "Taking training set from stdin\n");
    while (!feof(stdin)) {
        double *value = calloc(params->dim, sizeof(double));
        for (size_t d = 0; d < params->dim; d++) {
            scanf("%lf,", &value[d]);
        }
        char class;
        scanf("%c", &class);
        //
        size_t l;
        for (l = 0; classes[l] != '\0'; l++)
            if (classes[l] == class)
                break;
        if (classes[l] == '\0') {
            nClasses++;
            classes[l] = class;
        }
        assert(nClasses < 254);
        classesSize[l]++;
        trainingSetByClass[l] = realloc(trainingSetByClass[l], classesSize[l] * sizeof(Example));
        Example *ex = &trainingSetByClass[l][classesSize[l] -1];
        //
        ex->id = id;
        ex->val = value;
        ex->label = class;
        id++;
        // if (id > 71990) fprintf(stderr, "Ex(id=%d, val=%le, class=%c)\n", ex->id, ex->val[0], ex->class);
        //
        int hasEmptyline;
        scanf("\n%n", &hasEmptyline);
        if (hasEmptyline == 2) break;
    }
    //
    fprintf(stderr, "Training %u examples with %d classes (%s)\n", id, nClasses, classes);
    if (id < params->k) {
        errx(EXIT_FAILURE, "Not enough examples for training. At "__FILE__":%d\n", __LINE__);
    }
    Model *model = calloc(1, sizeof(Model));
    model->size = 0;
    model->clusters = calloc(1, sizeof(Cluster));
    for (unsigned int l = 0; l < nClasses; l++) {
        Example *trainingSet = trainingSetByClass[l];
        unsigned int trainingSetSize = classesSize[l];
        char class = classes[l];
        fprintf(stderr, "Training %u examples from class %c\n", trainingSetSize, class);
        Cluster *clusters = clustering(params, trainingSet, trainingSetSize, model->size);
        //
        unsigned int prevSize = model->size;
        model->size += params->k;
        model->clusters = realloc(model->clusters, model->size * sizeof(Cluster));
        for (size_t k = 0; k < params->k; k++) {
            clusters[k].label = class;
            model->clusters[prevSize + k] = clusters[k];
        }
        free(clusters);
    }
    return model;
}

Match *identify(MinasParams *params, MinasState *state, Example *example, Match *match) {
    Model *model = &state->model;
    assertMsg(model->size > 0, "Can't find nearest in model(%d).", model->size);
    match->pointId = example->id;
    match->label = MINAS_UNK_LABEL;
    match->distance = nearestClusterVal(params->dim, model->clusters, model->size, example->val, &match->cluster, 1);
    if (match->cluster == NULL) {
        restoreSleep(params, state);
        match->distance = nearestClusterVal(params->dim, model->clusters, model->size, example->val, &match->cluster, 0);
    }
    assertMsg(match->cluster != NULL, "Can't find nearest in model(%d).", model->size);
    match->cluster->latest_match_id = example->id;
    if (match->distance <= match->cluster->radius) {
        match->isMatch = 1;
        match->label = match->cluster->label;
        match->cluster->n_matches++;
    } else {
        match->isMatch = 0;
        match->cluster->n_misses++;
    }
    return match;
}

unsigned int noveltyDetection(MinasParams *params, MinasState *state, unsigned int *noveltyCount) {
    Cluster *clusters = clustering(params, state->unknowns, state->unknownsSize, state->model.size + 1);
    unsigned int extensions = 0, novelties = 0;
    Model *model = &(state->model);
    for (unsigned int k = 0; k < params->k; k++) {
        if (clusters[k].n_matches < params->minExamplesPerCluster) continue;
        //
        Cluster *nearest = NULL;
        double minDist = nearestClusterVal(params->dim, model->clusters, model->size, clusters[k].center, &nearest, 0);
        assertMsg(nearest != NULL, "Didn't find nearest in model(%d).", model->size);
        if (minDist <= params->noveltyF * nearest->distanceStdDev) {
            clusters[k].label = nearest->label;
            clusters[k].extensionOF = nearest->id;
            extensions++;
        } else {
            clusters[k].label = state->nextLabel;
            // inc label
            do {
                state->nextLabel++;
            } while (state->nextLabel < 128 && (isalpha(state->nextLabel) || state->nextLabel == '-'));
            // fprintf(stderr, "Novelty %s\n", printableLabel(clusters[k].label));
            novelties++;
        }
        //
        clusters[k].id = model->size;
        model->size++;
        model->clusters = realloc(model->clusters, model->size * sizeof(Cluster));
        model->clusters[model->size - 1] = clusters[k];
    }
    // unsigned int earliestId = unknowns[0].id;
    // unsigned int latestId = unknowns[unknownsSize -1].id;
    // fprintf(stderr, "ND clusters (%u, %u): %d extensions, %d novelties\n", earliestId, latestId, extensions, novelties);
    free(clusters);
    if (noveltyCount != NULL) {
        *noveltyCount = novelties;
    }
    return novelties + extensions;
}

unsigned int minasHandleUnknown(MinasParams *params, MinasState *state, Example *example) {
    unsigned int nNewClusters = 0;
    state->currId = example->id;
    state->unknowns[state->unknownsSize].id = example->id;
    for (size_t i = 0; i < params->dim; i++) {
        state->unknowns[state->unknownsSize].val[i] = example->val[i];
    }
    state->unknownsSize++;
    //
    if (state->unknownsSize == params->unknownsMaxSize ||
        (state->unknownsSize >= params->noveltyDetectionTrigger && state->currId - state->lastNDCheck > params->noveltyDetectionTrigger)
    ) {
        restoreSleep(params, state);
        unsigned int noveltyCount;
        unsigned int prevSize = state->model.size;
        nNewClusters = noveltyDetection(params, state, &noveltyCount);
        //
        unsigned long garbageCollected = 0, consumed = 0, reclassified = 0;
        for (unsigned long int ex = 0; ex < state->unknownsSize; ex++) {
            // compress
            unsigned long offset = garbageCollected + consumed + reclassified;
            if (offset > 0) {
                state->unknowns[ex - offset].id = state->unknowns[ex].id;
                for (size_t d = 0; d < params->dim; d++) {
                    state->unknowns[ex - offset].val[d] = state->unknowns[ex].val[d];
                }
            }
            // discart if consumed by new clusters in model
            Cluster *nearest;
            double distance = nearestClusterVal(params->dim, &state->model.clusters[prevSize], nNewClusters, state->unknowns[ex].val, &nearest, 0);
            assert(nearest != NULL);
            if (distance <= nearest->distanceMax) {
                consumed++;
                continue;
            }
            // discart if can be classified by previous model (aka, classifier model is behind this model)
            // distance = nearestClusterVal(params->dim, state->model.clusters, state->model.size - nNewClusters, unknowns[ex].val, &nearest);
            // assert(nearest != NULL);
            // if (distance <= nearest->distanceMax) {
            //     reclassified++;
            //     continue;
            // }
            // discart if example was present in last novelty detection (second round)
            if (state->unknowns[ex].id < state->lastNDCheck) {
                garbageCollected++;
                continue;
            }
        }
        unsigned long prevUnkSize = state->unknownsSize;
        state->unknownsSize -= (garbageCollected + consumed + reclassified);
        fprintf(stderr, "Novelties %3u, Extensions %3u, Unknowns %6lu, consumed %6lu, reclassified %6lu, garbageCollected %6lu\n",
                noveltyCount, nNewClusters - noveltyCount, prevUnkSize, consumed, reclassified, garbageCollected);
        state->lastNDCheck = state->currId;
    }
    // assert(state->unknownsSize < params->unknownsMaxSize);
    if (state->unknownsSize >= params->unknownsMaxSize) {
        unsigned long int garbageCollected = 1;
        for (unsigned long int ex = 1; ex < state->unknownsSize; ex++) {
            // compress
            if (garbageCollected > 0) {
                state->unknowns[ex - garbageCollected].id = state->unknowns[ex].id;
                for (size_t d = 0; d < params->dim; d++) {
                    state->unknowns[ex - garbageCollected].val[d] = state->unknowns[ex].val[d];
                }
            }
            // discart if example was present in last novelty detection
            state->unknowns[ex - garbageCollected] = state->unknowns[ex];
            if (state->unknowns[ex].id < state->lastNDCheck) {
                garbageCollected++;
                continue;
            }
        }
        state->unknownsSize -= garbageCollected;
        fprintf(stderr, "garbageCollected %6lu\n", garbageCollected);
    }
    return nNewClusters;
}

void minasHandleSleep(MinasParams *params, MinasState *state) {
    if (state->currId == 0) {
        return;
    }
    unsigned long idDiff = state->currId - state->lastForgetCheck;
    unsigned long active = state->model.size - state->sleepCounter;
    if (active <= 2 * params->k || idDiff < params->thresholdForgettingPast) {
        return;
    }
    for (unsigned int k = 0; k < state->model.size; k++) {
        Cluster *cl = &(state->model.clusters[k]);
        if (cl->isSleep) {
            continue;
        }
        if ((state->currId - cl->latest_match_id) > params->thresholdForgettingPast) {
            state->sleepCounter++;
            cl->isSleep = 1;
        }
        if (state->sleepCounter >= 2 *params->k) {
            break;
        }
    }
    // fprintf(stderr, "Forget: k %4u, model %4u, sleep %4u\n", params->k, state->model.size, state->sleepCounter);
}

void restoreSleep(MinasParams *params, MinasState *state) {
    if (state->sleepCounter == 0) {
        return;
    }
    for (unsigned int k = 0; k < state->model.size; k++) {
        Cluster *cl = &(state->model.clusters[k]);
        cl->latest_match_id = state->currId;
        cl->isSleep = 0;
    }
    state->sleepCounter = 0;
    // fprintf(stderr, "Restore: k %4u, model %4u, sleep %4u\n", params->k, state->model.size, state->sleepCounter);
}

char *labelMatchStatistics(Model *model, char *stats) {
    int nLabels = 0;
    unsigned int *labels = calloc(model->size, sizeof(unsigned int));
    unsigned long int *matches = calloc(model->size, sizeof(unsigned long int));
    unsigned long int nMatches = 0, nMisses = 0;
    for (unsigned int i = 0; i < model->size; i++) {
        Cluster *cl = &(model->clusters[i]);
        //
        size_t j = 0;
        for (; labels[j] != cl->label && labels[j] != '\0' && j < nLabels; j++);
        if (labels[j] == '\0') nLabels++;
        labels[j] = cl->label;
        nMatches += cl->n_matches;
        matches[j] += cl->n_matches;
        nMisses += cl->n_misses;
    }
    int statsIdx = sprintf(stats, "items: %10lu, hits: %10lu, misses: %10lu", nMatches + nMisses, nMatches, nMisses);
    char label[20];
    int printed = 0;
    for (size_t j = 0; labels[j] != '\0'; j++) {
        // if (matches[j] == 0) continue;
        printableLabelReuse(labels[j], label);
        statsIdx += sprintf(&stats[statsIdx], ",%c'%4s': %10lu", (printed % 5 == 0) ? '\n' : ' ', label, matches[j]);
        printed++;
    }
    free(labels);
    free(matches);
    return stats;
}

int printCluster(int dim, Cluster *cl) {
    int count = 0;
    char *out = calloc(400, sizeof(char));
    count += sprintf(&out[count], "Cluster: %20lu, %.4s, %10u, %le, %le, %le",
                     cl->id, printableLabel(cl->label), cl->n_matches,
                     cl->distanceAvg, cl->distanceStdDev, cl->radius);
    assertNotNull(cl->center);
    for (unsigned int d = 0; d < dim; d++)
        count += sprintf(&out[count], ", %le", cl->center[d]);
    count += sprintf(&out[count], "\n");
    // fwrite(out, sizeof(char), count, stdout);
    printf("%s", out);
    fflush(stdout);
    free(out);
    return count;
}

int readCluster(int kParam, int dim, Cluster *cluster, char lineptr[]) {
    int readCur = 0, readTot = 0;
    char *labelString;
    // line is on format "Cluster: %10u, %s, %10u, %le, %le, %le" + dim * ", %le"
    int assigned = sscanf(
        lineptr, "Cluster: %20lu, %m[^,], %10u, %le, %le, %le%n",
        &cluster->id, &labelString, &cluster->n_matches,
        &cluster->distanceAvg, &cluster->distanceStdDev, &cluster->radius,
        &readCur);
    assertMsg(assigned == 6, "Got %d assignments.", assigned);
    readTot += readCur;
    cluster->label = fromPrintableLabel(labelString);
    free(labelString);
    // cluster->center = calloc(dim, sizeof(double));
    for (size_t d = 0; d < dim; d++) {
        assert(sscanf(&lineptr[readTot], ", %le%n", &(cluster->center[d]), &readCur));
        readTot += readCur;
    }
    return readTot;
}

Cluster *addCluster(MinasParams *params, MinasState *state, Cluster *cluster) {
    assert(cluster != NULL);
    assert(state->model.clusters != NULL);
    Model *model = &state->model;
    if (model->size > 0 && model->size % params->k == 0) {
        fprintf(stderr, "realloc model %d\n", model->size + params->k);
        model->clusters = realloc(model->clusters, (model->size + params->k) * sizeof(Cluster));
    }
    unsigned int index = model->size;
    model->size++;
    model->clusters[index] = *cluster;
    model->clusters[index].center = calloc(params->dim, sizeof(double));
    for (size_t d = 0; d < params->dim; d++) {
        model->clusters[index].center[d] = cluster->center[d];
    }
    if (!isalpha(cluster->label) && state->nextLabel <= cluster->label) {
        state->nextLabel = cluster->label + 1;
    }
    return &model->clusters[index];
}

char getMfogLine(FILE *fd, char **line, size_t *lineLen, unsigned int kParam, unsigned int dim, Cluster *cluster, Example *example) {
    if (feof(fd)) {
        return 0;
    }
    getline(line, lineLen, fd);
    int readCur = 0, readTot = 0;
    char ret;
    Cluster *cl = cluster;
    if (cluster == NULL) {
        cl = calloc(1, sizeof(Cluster));
        cl->center = calloc(dim, sizeof(double));
    }
    switch (*line[0]) {
    case 'C':
        readCluster(kParam, dim, cl, *line);
        // cl->latest_match_id = *id;
        cl->isIntrest = 1;
        cl->n_matches = 0;
        cl->n_misses = 0;
        ret = 'C';
        break;
    case 'U':
        assert(sscanf(*line, "Unknown: %20lu%n", &example->id, &readCur));
        readTot += readCur;
        for (size_t d = 0; d < dim; d++) {
            assertMsg(sscanf(&(*line)[readTot], ", %le%n", &example->val[d], &readCur), "Didn't understand '%s'.", &(*line)[readTot]);
            readTot += readCur;
        }
        // if (example->id > *id) {
        //     *id = example->id;
        // }
        ret = 'U';
        break;
    case '#':
        ret = '#';
        break;
    default:
        for (unsigned int d = 0; d < dim; d++) {
            int scanned = sscanf(&((*line)[readTot]), "%lf,%n", &example->val[d], &readCur);
            if (!scanned) {
                errx(EXIT_FAILURE, "Read error with line '%s' and d=%u. At "__FILE__":%d\n", *line, d, __LINE__);
            }
            readTot += readCur;
        }
        // ignore class
        // example->id = *id;
        // (*id)++;
        ret = 'E';
        break;
    }
    if (cluster == NULL) {
        free(cl->center);
        free(cl);
    }
    return ret;
}

long double timespecdiff(struct timespec *a, struct timespec *b, struct timespec *diff) {
    long tv_sec = a->tv_sec - b->tv_sec;
    long tv_nsec = a->tv_nsec - b->tv_nsec;
    if (diff != NULL) {
        diff->tv_sec = tv_sec;
        diff->tv_nsec = tv_nsec;
    }
    return tv_sec + tv_nsec / 1e9;
}

#endif // _BASE_C
