#ifndef _BASE_C
#define _BASE_C 1

#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
// #include <unistd.h>
#include <ctype.h>
#include <math.h>

#include "./base.h"

char *printableLabelReuse(unsigned int label, char *ret) {
    if (isalpha(label) || label == '-') {
        ret[0] = label;
        ret[1] = '\0';
    } else {
        sprintf(ret, "%4u", label);
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

double nearestClusterVal(int dim, ModelLink *head, unsigned int limit, double val[], Cluster **nearest) {
    double minDist;
    *nearest = NULL;
    ModelLink *curr = head;
    for (unsigned int k = 0; curr != NULL && k < limit; k++, curr = curr->next) {
        double dist = 0.0;
        for (unsigned int d = 0; d < dim; d++) {
            double v = (curr->cluster.center[d] - val[d]);
            // dist += fabs(v);
            dist += v * v;
            if (k > 0 && dist > minDist) break;
        }
        if (k == 0 || dist <= minDist) {
            minDist = dist;
            *nearest = &(curr->cluster);
        }
    }
    return sqrt(minDist);
}

ModelLink *kMeansInit(int kParam, int dim, Example trainingSet[], unsigned int trainingSetSize, unsigned int initalId) {
    if (trainingSetSize < kParam) {
        errx(EXIT_FAILURE, "Not enough examples for K-means. At "__FILE__":%d\n", __LINE__);
    }
    ModelLink *cur, *head = calloc(1, sizeof(ModelLink));
    cur = head;
    for (size_t i = 0; i < kParam; i++) {
        if (i > 0) {
            cur->next = calloc(1, sizeof(ModelLink));
            cur = cur->next;
        }
        cur->cluster.id = initalId + i;
        cur->cluster.n_matches = 0;
        cur->cluster.center = calloc(dim, sizeof(double));
        cur->cluster.ls_valLinearSum = calloc(dim, sizeof(double));
        cur->cluster.ss_valSquareSum = calloc(dim, sizeof(double));
        for (size_t d = 0; d < dim; d++) {
            cur->cluster.center[d] = trainingSet[i].val[d];
            cur->cluster.ls_valLinearSum[d] = trainingSet[i].val[d];
            cur->cluster.ss_valSquareSum[d] = trainingSet[i].val[d] * trainingSet[i].val[d];
        }
    }
    return head;
}

double kMeans(int kParam, int dim, double precision, ModelLink* head, Example trainingSet[], unsigned int trainingSetSize) {
    double improvement, prevGlobalDistance, globalDistance = dim * kParam * trainingSetSize * 2;
    unsigned int iteration = 0;
    do {
        prevGlobalDistance = globalDistance;
        globalDistance = 0.0;
        for (unsigned int i = 0; i < trainingSetSize; i++) {
            Cluster *nearest = NULL;
            double minDist = nearestClusterVal(dim, head, kParam, trainingSet[i].val, &nearest);
            globalDistance += minDist;
            nearest->n_matches++;
            for (unsigned int d = 0; d < dim; d++) {
                nearest->ls_valLinearSum[d] += trainingSet[i].val[d];
                nearest->ss_valSquareSum[d] += trainingSet[i].val[d] * trainingSet[i].val[d];
            }
        }
        ModelLink* curr = head;
        for (unsigned int k = 0; curr != NULL || k < kParam; k++, curr = curr->next) {
            for (unsigned int d = 0; d < dim; d++) {
                if (curr->cluster.n_matches > 0)
                    curr->cluster.center[d] = curr->cluster.ls_valLinearSum[d] / curr->cluster.n_matches;
                curr->cluster.ls_valLinearSum[d] = 0.0;
                curr->cluster.ss_valSquareSum[d] = 0.0;
            }
            curr->cluster.n_matches = 0;
        }
        improvement = globalDistance - prevGlobalDistance;
        // fprintf(stderr, "\t[%3u] k-Means %le -> %le (%+le / %+le)\n", iteration, prevGlobalDistance, globalDistance, improvement, precision);
        iteration++;
    } while (prevGlobalDistance != globalDistance && fabs(improvement) > precision && iteration < 100);
    return globalDistance;
}

ModelLink *clustering(MinasParams *params, Example trainingSet[], unsigned int trainingSetSize, unsigned int initalId) {
    ModelLink *head = kMeansInit(params->k, params->dim, trainingSet, trainingSetSize, initalId);
    kMeans(params->k, params->dim, params->precision, head, trainingSet, trainingSetSize);
    //
    double *distances = calloc(trainingSetSize, sizeof(double));
    Cluster **n_matches = calloc(trainingSetSize, sizeof(Cluster *));
    ModelLink *curr = head;
    for (unsigned int k = 0; curr != NULL && k < params->k; k++, curr = curr->next) {
        curr->cluster.radius = 0.0;
        curr->cluster.n_matches = 0;
        curr->cluster.distanceMax = 0.0;
        curr->cluster.distanceLinearSum = 0.0;
        curr->cluster.distanceSquareSum = 0.0;
    }
    for (unsigned int i = 0; i < trainingSetSize; i++) {
        Cluster *nearest = NULL;
        double minDist = nearestClusterVal(params->dim, head, params->k, trainingSet[i].val, &nearest);
        distances[i] = minDist;
        n_matches[i] = nearest;
        // if (exampleToClusterMap != NULL) {
        //     exampleToClusterMap[i] = nearest->id;
        // }
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
    curr = head;
    for (unsigned int k = 0; curr != NULL && k < params->k; k++) {
        if (curr->cluster.n_matches == 0) {
            ModelLink *toFree = curr;
            curr = curr->next;
            free(toFree->cluster.ls_valLinearSum);
            free(toFree->cluster.ss_valSquareSum);
            free(toFree);
            continue;
        }
        curr->cluster.distanceAvg = curr->cluster.distanceLinearSum / curr->cluster.n_matches;
        curr->cluster.distanceStdDev = 0.0;
        for (unsigned int i = 0; i < trainingSetSize; i++) {
            if (n_matches[i] == &curr->cluster) {
                double p = distances[i] - curr->cluster.distanceAvg;
                curr->cluster.distanceStdDev += p * p;
            }
        }
        curr->cluster.distanceStdDev = sqrt(curr->cluster.distanceStdDev);
        curr->cluster.radius = params->radiusF * curr->cluster.distanceStdDev;
        free(curr->cluster.ls_valLinearSum);
        free(curr->cluster.ss_valSquareSum);
        curr = curr->next;
    }
    free(distances);
    free(n_matches);
    return head;
}

Model *training(MinasParams *params) {
    // read training stream
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
    model->nextLabel = 0;
    model->head = NULL;
    model->tail = model->head;
    for (unsigned int l = 0; l < nClasses; l++) {
        Example *trainingSet = trainingSetByClass[l];
        unsigned int trainingSetSize = classesSize[l];
        char class = classes[l];
        fprintf(stderr, "Training %u examples from class %c\n", trainingSetSize, class);
        ModelLink *head = clustering(params, trainingSet, trainingSetSize, model->nextId);
        //
        model->size += params->k;
        model->nextId += params->k;
        if (model->head == NULL) {
            model->head = head;
        } else {
            model->tail->next = head;
        }
        for (model->tail = head; model->tail->next != NULL; model->tail = model->tail->next) {}
    }
    return model;
}

Match *identify(MinasParams *params, Model *model, Example *example, Match *match) {
    // Match *match = calloc(1, sizeof(Match));
    assertMsg(model->size > 0, "Can't find nearest in model(%d).", model->size);
    match->pointId = example->id;
    match->label = MINAS_UNK_LABEL;
    match->distance = nearestClusterVal(params->dim, model->head, model->size, example->val, &match->cluster);
    assertMsg(match->cluster != NULL, "Can't find nearest in model(%d).", model->size);
    if (match->distance <= match->cluster->radius) {
        match->isMatch = 1;
        match->label = match->cluster->label;
        match->cluster->n_matches++;
        match->cluster->latest_match_id = example->id;
    } else {
        match->isMatch = 0;
        match->cluster->n_misses++;
    }
    return match;
}

unsigned int noveltyDetection(MinasParams *params, MinasState *state, unsigned int *noveltyCount) {
    marker("noveltyDetection");
    ModelLink *head = clustering(params, state->unknowns, state->unknownsSize, state->model.size + 1);
    unsigned int extensions = 0, novelties = 0;
    marker("evaluate clusters");
    for (ModelLink *curr = head; curr != NULL; ) {
        fprintf(stderr, "evaluate cluster %u "__FILE__":%d\n", curr->cluster.id, __LINE__);
        if (curr->cluster.n_matches < params->minExamplesPerCluster) {
            ModelLink *toFree = curr;
            curr = curr->next;
            free(toFree);
            continue;
        }
        // check if extension or novelty
        Cluster *nearest = NULL;
        double minDist = nearestClusterVal(params->dim, state->model.head, state->model.size, curr->cluster.center, &nearest);
        assertMsg(nearest != NULL, "Didn't find nearest in model(%d).", state->model.size);
        if (minDist <= params->noveltyF * nearest->distanceStdDev) {
            curr->cluster.label = nearest->label;
            curr->cluster.extensionOF = nearest->id;
            extensions++;
        } else {
            curr->cluster.label = state->model.nextLabel;
            // inc label
            do {
                state->model.nextLabel++;
            } while (isalpha(state->model.nextLabel) || state->model.nextLabel == '-');
            // fprintf(stderr, "Novelty %s\n", printableLabel(curr->cluster.label));
            novelties++;
        }
        //
        curr->cluster.id = state->model.nextId;
        state->model.nextId++;
        curr->cluster.n_matches = 0;
        curr->cluster.n_misses = 0;
        curr->cluster.latest_match_id = state->currId;
        //
        state->model.size++;
        ModelLink *toAdd = curr;
        curr = curr->next;
        if (state->model.head == NULL) {
            state->model.head = toAdd;
            state->model.tail = toAdd;
        } else {
            state->sleep.tail->next = toAdd;
            state->model.tail = toAdd;
        }
        state->model.tail->next = NULL;
    }
    // unsigned int earliestId = unknowns[0].id;
    // unsigned int latestId = unknowns[unknownsSize -1].id;
    // fprintf(stderr, "ND clusters (%u, %u): %d extensions, %d novelties\n", earliestId, latestId, extensions, novelties);
    if (noveltyCount != NULL) {
        *noveltyCount = novelties;
    }
    return novelties + extensions;
}

unsigned int minasHandleUnknown(MinasParams *params, MinasState *state, Example *example) {
    unsigned int nNewClusters = 0;
    state->currId = example->id;
    // Example *unknowns = state->unknowns;
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
        // unsigned int prevSize = state->model.size;
        ModelLink *prevTail = state->model.tail;
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
            double distance = nearestClusterVal(params->dim, prevTail->next, nNewClusters, state->unknowns[ex].val, &nearest);
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
        state->unknownsSize -= (garbageCollected + consumed + reclassified);
        fprintf(stderr, "Novelties %3u, Extensions %3u, consumed %6lu, reclassified %6lu, garbageCollected %6lu\n",
                noveltyCount, nNewClusters - noveltyCount, consumed, reclassified, garbageCollected);
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
    if (state->currId > 0 && (state->currId - state->lastForgetCheck) > params->thresholdForgettingPast) {
        marker("minasHandleSleep");
        for (ModelLink *curr = state->model.head; curr != NULL && state->model.size > params->k; ) {
            ModelLink *moved = curr;
            curr = curr->next;
            if (state->currId - moved->cluster.latest_match_id > params->thresholdForgettingPast) {
                // move
                if (state->sleep.head == NULL) {
                    state->sleep.head = moved;
                    state->sleep.tail = moved;
                } else {
                    state->sleep.tail->next = moved;
                    state->model.tail = moved;
                }
                state->sleep.tail->next = NULL;
                state->sleep.size++;
                state->model.size--;
            }
        }
        fprintf(stderr, "Forget Mechanic: model %4u, sleep %4u\n", state->model.size, state->sleep.size);
    }
}

void restoreSleep(MinasParams *params, MinasState *state) {
    if (state->sleep.size == 0) {
        return;
    }
    marker("restoreSleep");
    state->model.tail->next = state->sleep.head;
    state->sleep.head = NULL;
    state->sleep.tail = NULL;
    state->sleep.size = 0;
    state->model.size += state->sleep.size;
}

char *labelMatchStatistics(Model *model, char *stats) {
    int nLabels = 0;
    unsigned int *labels = calloc(model->size, sizeof(unsigned int));
    unsigned long int *matches = calloc(model->size, sizeof(unsigned long int));
    unsigned long int nMatches = 0, nMisses = 0;
    for (ModelLink *curr = model->head; curr != NULL; ) {
        Cluster *cl = &curr->cluster;
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
    count += sprintf(&out[count], "Cluster: %10u, %.4s, %10u, %le, %le, %le",
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
        lineptr, "Cluster: %10u, %m[^,], %10u, %le, %le, %le%n",
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
Cluster *addCluster(int dim, Cluster *cluster, Model *model) {
    assert(cluster != NULL);
    assert(model != NULL);
    model->size++;
    if (model->tail == NULL) {
        model->head = calloc(1, sizeof(ModelLink));
        model->tail = model->head;
    } else {
        model->tail->next = calloc(1, sizeof(ModelLink));
        model->tail = model->tail->next;
    }
    model->tail->cluster = *cluster;
    cluster->center = calloc(dim, sizeof(double));
    if (!isalpha(cluster->label) && model->nextLabel <= cluster->label) {
        model->nextLabel = cluster->label + 1;
    }
    return &(model->tail->cluster);
}

char getMfogLine(FILE *fd, char **line, size_t *lineLen, unsigned int kParam, unsigned int dim, unsigned long *id, Model *model, Cluster *cluster, Example *example) {
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
        cl->latest_match_id = *id;
        cl->isIntrest = 1;
        cl->n_matches = 0;
        cl->n_misses = 0;
        ret = 'C';
        break;
    case 'U':
        assert(sscanf(*line, "Unknown: %10u%n", &example->id, &readCur));
        readTot += readCur;
        for (size_t d = 0; d < dim; d++) {
            assertMsg(sscanf(&(*line)[readTot], ", %le%n", &example->val[d], &readCur), "Didn't understand '%s'.", &(*line)[readTot]);
            readTot += readCur;
        }
        if (example->id > *id) {
            *id = example->id;
        }
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
        example->id = *id;
        (*id)++;
        ret = 'E';
        break;
    }
    if (cluster == NULL) {
        free(cl->center);
        free(cl);
    }
    return ret;
}

#endif // _BASE_C
