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

double nearestClusterVal(int dim, Cluster clusters[], size_t nClusters, double val[], Cluster **nearest, unsigned int thresholdForgettingPast, unsigned int currentId) {
    double minDist;
    *nearest = NULL;
    for (size_t k = 0; k < nClusters; k++) {
        // if (thresholdForgettingPast > 0 && currentId - clusters[k].latest_match_id > thresholdForgettingPast) {
        //     continue;
        // }
        double dist = 0.0;
        for (size_t d = 0; d < dim; d++) {
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
    Cluster *clusters = calloc(kParam, sizeof(Cluster));
    for (size_t i = 0; i < kParam; i++) {
        clusters[i].id = initalId + i;
        clusters[i].n_matches = 0;
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

double kMeans(int kParam, int dim, double precision, Cluster* clusters, Example trainingSet[], unsigned int trainingSetSize) {
    // clock_t start = clock();
    double improvement, prevGlobalDistance, globalDistance = dim * kParam * trainingSetSize * 2;
    unsigned int iteration = 0;
    do {
        prevGlobalDistance = globalDistance;
        globalDistance = 0.0;
        for (size_t i = 0; i < trainingSetSize; i++) {
            Cluster *nearest = NULL;
            double minDist = nearestClusterVal(dim, clusters, kParam, trainingSet[i].val, &nearest, 0, 0);
            globalDistance += minDist;
            nearest->n_matches++;
            for (size_t d = 0; d < dim; d++) {
                nearest->ls_valLinearSum[d] += trainingSet[i].val[d];
                nearest->ss_valSquareSum[d] += trainingSet[i].val[d] * trainingSet[i].val[d];
            }
        }
        for (size_t k = 0; k < kParam; k++) {
            for (size_t d = 0; d < dim; d++) {
                if (clusters[k].n_matches > 0)
                    clusters[k].center[d] = clusters[k].ls_valLinearSum[d] / clusters[k].n_matches;
                clusters[k].ls_valLinearSum[d] = 0.0;
                clusters[k].ss_valSquareSum[d] = 0.0;
            }
            clusters[k].n_matches = 0;
        }
        improvement = globalDistance - prevGlobalDistance;
        // fprintf(stderr, "\t[%3u] k-Means %le -> %le (%+le)\n", iteration, prevGlobalDistance, globalDistance, improvement);
        iteration++;
    } while (fabs(improvement) > precision && iteration < 100);
    // printTiming(kMeans, trainingSetSize);
    return globalDistance;
}

Cluster* clustering(int kParam, int dim, double precision, double radiusF, Example trainingSet[], unsigned int trainingSetSize, unsigned int initalId) {
    Cluster *clusters = kMeansInit(kParam, dim, trainingSet, trainingSetSize, initalId);
    kMeans(kParam, dim, precision, clusters, trainingSet, trainingSetSize);
    //
    double *distances = calloc(trainingSetSize, sizeof(double));
    Cluster **n_matches = calloc(trainingSetSize, sizeof(Cluster *));
    for (size_t k = 0; k < kParam; k++) {
        clusters[k].radius = 0.0;
        clusters[k].n_matches = 0;
        clusters[k].distanceMax = 0.0;
        clusters[k].distanceLinearSum = 0.0;
        clusters[k].distanceSquareSum = 0.0;
    }
    for (size_t i = 0; i < trainingSetSize; i++) {
        Cluster *nearest = NULL;
        double minDist = nearestClusterVal(dim, clusters, kParam, trainingSet[i].val, &nearest, 0, 0);
        distances[i] = minDist;
        n_matches[i] = nearest;
        //
        nearest->n_matches++;
        nearest->distanceLinearSum += minDist;
        nearest->distanceSquareSum += minDist * minDist;
        if (minDist > nearest->distanceMax) {
            nearest->distanceMax = minDist;
        }
        for (size_t d = 0; d < dim; d++) {
            nearest->ls_valLinearSum[d] += trainingSet[i].val[d];
            nearest->ss_valSquareSum[d] += trainingSet[i].val[d] * trainingSet[i].val[d];
        }
    }
    for (size_t k = 0; k < kParam; k++) {
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
        clusters[k].radius = radiusF * clusters[k].distanceStdDev;
        free(clusters[k].ls_valLinearSum);
        free(clusters[k].ss_valSquareSum);
    }
    free(distances);
    free(n_matches);
    return clusters;
}

Model *training(int kParam, int dim, double precision, double radiusF) {
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
        double *value = calloc(dim, sizeof(double));
        for (size_t d = 0; d < dim; d++) {
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
    if (id < kParam) {
        errx(EXIT_FAILURE, "Not enough examples for training. At "__FILE__":%d\n", __LINE__);
    }
    Model *model = calloc(1, sizeof(Model));
    model->size = 0;
    model->nextLabel = 0;
    model->clusters = calloc(1, sizeof(Cluster));
    for (size_t l = 0; l < nClasses; l++) {
        Example *trainingSet = trainingSetByClass[l];
        unsigned int trainingSetSize = classesSize[l];
        char class = classes[l];
        fprintf(stderr, "Training %u examples from class %c\n", trainingSetSize, class);
        Cluster *clusters = clustering(kParam, dim, precision, radiusF, trainingSet, trainingSetSize, model->size);
        //
        unsigned int prevSize = model->size;
        model->size += kParam;
        model->clusters = realloc(model->clusters, model->size * sizeof(Cluster));
        for (size_t k = 0; k < kParam; k++) {
            clusters[k].label = class;
            model->clusters[prevSize + k] = clusters[k];
        }
        free(clusters);
    }
    return model;
}

Match *identify(int kParam, int dim, double precision, double radiusF, Model *model, Example *example, Match *match, unsigned int thresholdForgettingPast) {
    // Match *match = calloc(1, sizeof(Match));
    assertMsg(model->size > 0, "Can't find nearest in model(%d).", model->size);
    match->pointId = example->id;
    match->label = MINAS_UNK_LABEL;
    match->distance = nearestClusterVal(dim, model->clusters, model->size, example->val, &match->cluster, thresholdForgettingPast, example->id);
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
Cluster *addCluster(int kParam, int dim, Cluster *cluster, Model *model) {
    if (model->size > 0 && model->size % kParam == 0) {
        fprintf(stderr, "realloc model %d\n", model->size + kParam);
        model->clusters = realloc(model->clusters, (model->size + kParam) * sizeof(Cluster));
    }
    model->clusters[model->size] = *cluster;
    model->size++;
    model->clusters[model->size].center = calloc(dim, sizeof(double));
    for (size_t d = 0; d < dim; d++) {
        model->clusters[model->size].center[d] = cluster->center[d];
    }
    if (!isalpha(cluster->label) && model->nextLabel <= cluster->label) {
        model->nextLabel = cluster->label + 1;
    }
    return &model->clusters[model->size];
}

char getMfogLine(FILE *fd, char **line, unsigned long *lineLen, int kParam, int dim, unsigned long *id, Model *model, Cluster *cluster, Example *example) {
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
        for (size_t d = 0; d < dim; d++) {
            int scanned = sscanf(&((*line)[readTot]), "%lf,%n", &example->val[d], &readCur);
            if (!scanned) {
                errx(EXIT_FAILURE, "Read error with line '%s' and d=%lu. At "__FILE__":%d\n", *line, d, __LINE__);
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

unsigned int noveltyDetection(PARAMS_ARG, Model *model, Example *unknowns, size_t unknownsSize, unsigned int *noveltyCount) {
    Cluster *clusters = clustering(kParam, dim, precision, radiusF, unknowns, unknownsSize, model->size);
    unsigned int extensions = 0, novelties = 0;
    for (size_t k = 0; k < kParam; k++) {
        if (clusters[k].n_matches < minExamplesPerCluster) continue;
        //
        Cluster *nearest = NULL;
        double minDist = nearestClusterVal(dim, model->clusters, model->size, clusters[k].center, &nearest, 0, 0);
        assertMsg(nearest != NULL, "Didn't find nearest in model(%d).", model->size);
        if (minDist <= noveltyF * nearest->distanceStdDev) {
            clusters[k].label = nearest->label;
            clusters[k].extensionOF = nearest->id;
            extensions++;
        } else {
            clusters[k].label = model->nextLabel;
            // inc label
            do {
                model->nextLabel++;
            } while (model->nextLabel < 128 && (isalpha(model->nextLabel) || model->nextLabel == '-'));
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

char *labelMatchStatistics(Model *model, char *stats) {
    int nLabels = 0;
    unsigned int *labels = calloc(model->size, sizeof(unsigned int));
    unsigned long int *matches = calloc(model->size, sizeof(unsigned long int));
    unsigned long int nMatches = 0, nMisses = 0;
    for (size_t i = 0; i < model->size; i++) {
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

#endif // _BASE_C
