#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <math.h>
#include <time.h>
#include <ctype.h>

#define assertEquals(val, exp) \
    if (val != exp) errx(EXIT_FAILURE, "Assert error. At "__FILE__":%d\n", __LINE__)

#define assertDiffer(val, exp) \
    if (val == exp) errx(EXIT_FAILURE, "Assert error. At "__FILE__":%d\n", __LINE__)

#define printTiming(setSize) \
    fprintf(stderr, "[%s] %le seconds for %s(%u). At %s:%d\n", \
    params->executable, ((double)(clock() - start)) / 1000000.0, __FUNCTION__, setSize, __FILE__, __LINE__);

typedef struct {
    unsigned int k;
    unsigned int dim;
    unsigned int minExamplesPerCluster;
    double precision;
    double radiusF;
    double noveltyF;
    const char *executable;
    unsigned int useCluStream, cluStream_q_maxMicroClusters;
    double cluStream_time_threshold_delta_δ;
} Params;

typedef struct {
    unsigned int id;
    char class;
    double *val;
} Example;

typedef struct {
    unsigned int id, n_matches;
    char label;
    double *center;
    double *ls_valLinearSum, *ss_valSquareSum;
    // double *valAverage, *valStdDev;
    double distanceLinearSum, distanceSquareSum;
    double timeLinearSum, timeSquareSum;
    double distanceAvg, distanceStdDev, radius;
    double time_mu_μ, time_sigma_σ;
    // assumed last m arrivals in each micro-cluster to be __m = n__
    // so, the m/(2 · n)-th percentile is the 50th percentile
    // therefore z-indez is 0.0 and time_relevance_stamp is the mean distance;
    // double time_relevance_stamp_50;
    // unsigned int *ids, idSize; // used only to reconstruct clusters from snapshot
} Cluster;

typedef struct {
    Cluster *clusters;
    unsigned int size;
    unsigned int nextLabel;
} Model;

typedef struct {
    // int pointId, clusterId;
    // char clusterLabel, clusterCatergoy;
    // double clusterRadius;
    char label;
    double distance; // , secondDistance;
    Cluster *cluster;
    // Example *example;
    char *labelStr;
} Match;

double euclideanDistance(unsigned int dim, double a[], double b[]) {
    double distance = 0;
    for (size_t d = 0; d < dim; d++) {
        distance += (a[d] - b[d]) * (a[d] - b[d]);
    }
    return sqrt(distance);
}

Cluster* kMeansInit(Params *params, Example trainingSet[], unsigned int trainingSetSize, unsigned int initalId) {
    Cluster *clusters = calloc(params->k, sizeof(Cluster));
    for (size_t i = 0; i < params->k; i++) {
        clusters[i].id = initalId + i;
        clusters[i].n_matches = 0;
        clusters[i].center = calloc(params->dim, sizeof(double));
        clusters[i].ls_valLinearSum = calloc(params->dim, sizeof(double));
        clusters[i].ss_valSquareSum = calloc(params->dim, sizeof(double));
        // clusters[i].valAverage = calloc(params->dim, sizeof(double));
        // clusters[i].valStdDev = calloc(params->dim, sizeof(double));
        for (size_t d = 0; d < params->dim; d++) {
            clusters[i].center[d] = trainingSet[i].val[d];
            clusters[i].ls_valLinearSum[d] = 0.0;
            clusters[i].ls_valLinearSum[d] = 0.0;
            // clusters[i].valAverage[d] = 0.0;
            // clusters[i].valStdDev[d] = 0.0;
        }
    }
    return clusters;
}

double kMeans(Params *params, Cluster* clusters, Example trainingSet[], unsigned int trainingSetSize) {
    clock_t start = clock();
    double improvement, prevGlobalDistance, globalDistance = params->dim * params->k * trainingSetSize * 2;
    unsigned int iteration = 0;
    do {
        prevGlobalDistance = globalDistance;
        globalDistance = 0.0;
        for (size_t i = 0; i < trainingSetSize; i++) {
            double minDist = params->dim * 2;
            Cluster *nearest = NULL;
            for (size_t k = 0; k < params->k; k++) {
                double dist = euclideanDistance(params->dim, clusters[k].center, trainingSet[i].val);
                if (nearest == NULL || dist <= minDist) {
                    minDist = dist;
                    nearest = &clusters[k];
                }
            }
            globalDistance += minDist;
            nearest->n_matches++;
            for (size_t d = 0; d < params->dim; d++) {
                nearest->ls_valLinearSum[d] += trainingSet[i].val[d];
            }
        }
        for (size_t k = 0; k < params->k; k++) {
            for (size_t d = 0; d < params->dim; d++) {
                if (clusters[k].n_matches > 0)
                    clusters[k].center[d] = clusters[k].ls_valLinearSum[d] / clusters[k].n_matches;
                clusters[k].ls_valLinearSum[d] = 0.0;
                clusters[k].ss_valSquareSum[d] = 0.0;
            }
            clusters[k].n_matches = 0;
        }
        improvement = globalDistance - prevGlobalDistance;
        fprintf(stderr, "\t[%3u] k-Means %le -> %le (%+le)\n", iteration, prevGlobalDistance, globalDistance, improvement);
        if (improvement < 0)
            improvement = -improvement;
        iteration++;
    } while (improvement > params->precision && iteration < 100);
    printTiming(trainingSetSize);
    return globalDistance;
}

Cluster* cluStream(Params *params, Example trainingSet[], unsigned int trainingSetSize, unsigned int initalId) {
    clock_t start = clock();
    /**
     * m        Defines the maximum number of micro-clusters used in CluStream
     * horizon  Defines the time window to be used in CluStream
     * t        Maximal boundary factor (=Kernel radius factor)
     *          When deciding to add a new data point to a micro-cluster,
     *          the maximum boundary is defined as a factor of t of the RMS
     *          deviation of the data points in the micro-cluster from the centroid.
     * k        Number of macro-clusters to produce using weighted k-means. NULL disables automatic reclustering.
    **/
    // int alphaStorageFator = 2;
    // double horizon = 3;
    // double t = params->radiusF;
    //
    double k = params->k;
    params->k = params->cluStream_q_maxMicroClusters;
    double initNumbers = params->cluStream_q_maxMicroClusters * params->minExamplesPerCluster;
    Cluster *microClusters = kMeansInit(params, trainingSet, initNumbers, initalId);
    kMeans(params, microClusters, trainingSet, params->cluStream_q_maxMicroClusters);
    // restore k
    params->k = k;
    // fill radius with nearest cl dist
    for (size_t clId = 0; clId < params->cluStream_q_maxMicroClusters; clId++) {
        microClusters[clId].n_matches = 1;
        microClusters[clId].timeLinearSum = 1;
        microClusters[clId].timeSquareSum = 1;
        //
        // initial radius is distance to nearest cluster
        Cluster *nearest = NULL;
        for (size_t nearId = 0; nearId < params->cluStream_q_maxMicroClusters; nearId++) {
            double dist = euclideanDistance(params->dim, microClusters[clId].center, microClusters[clId].center);
            if (nearest == NULL || microClusters[clId].radius > dist) {
                microClusters[clId].radius = dist;
                nearest = &microClusters[clId];
            }
        }
    }
    // consume stream
    size_t nextClusterId = params->cluStream_q_maxMicroClusters;
    for (size_t i = 0; i < trainingSetSize; i++) {
        Example *ex = &trainingSet[i];
        //
        Cluster *nearest = NULL;
        double minDist;
        for (size_t clId = 0; clId < params->cluStream_q_maxMicroClusters; clId++) {
            double dist = euclideanDistance(params->dim, microClusters[clId].center, ex->val);
            if (nearest == NULL || minDist > dist) {
                minDist = dist;
                nearest = &microClusters[clId];
            }
        }
        if (nearest->radius < minDist) {
            // add example to cluster
            nearest->n_matches++;
            nearest->timeLinearSum += i;
            nearest->timeSquareSum += i * i;
            nearest->radius = 0;
            nearest->distanceAvg = 0;
            for (size_t d = 0; d < params->dim; d++) {
                nearest->ls_valLinearSum[d] += ex->val[d];
                nearest->ss_valSquareSum[d] += ex->val[d] * ex->val[d];
                //
                nearest->center[d] = nearest->ls_valLinearSum[d] / nearest->n_matches;
                //
                nearest->distanceAvg += nearest->ls_valLinearSum[d] / nearest->n_matches;
                //
                double variance = nearest->ls_valLinearSum[d] / nearest->n_matches;
                variance *= variance;
                variance -= nearest->ss_valSquareSum[d] / nearest->n_matches;
                if (variance < 0) {
                    variance = -variance;
                }
                nearest->radius += variance;
            }
            nearest->radius = nearest->distanceAvg + sqrt(nearest->radius) * params->radiusF;
            //
            nearest->time_mu_μ = nearest->timeLinearSum / nearest->n_matches;
            nearest->time_sigma_σ = sqrt((nearest->timeSquareSum / nearest->n_matches) - (nearest->time_mu_μ * nearest->time_mu_μ));
            // assumed last m arrivals in each micro-cluster to be __m = n__
            // so, the m/(2 · n)-th percentile is the 50th percentile
            // therefore z-indez is 0.0 and using `time_relevance_stamp = μ + z*σ`
            // time_relevance_stamp is the mean when the storage feature is not used;
        } else {
            // add new pattern, for a new cluster to enter, one must leave
            // can delete?
            Cluster *oldest = NULL;
            for (size_t clId = 0; clId < params->cluStream_q_maxMicroClusters; clId++) {
                // minimize relavance stamp
                if (oldest == NULL || microClusters[clId].time_mu_μ < oldest->time_mu_μ) {
                    oldest = &microClusters[clId];
                }
            }
            Cluster *deleted = NULL;
            if (oldest->time_mu_μ < params->cluStream_time_threshold_delta_δ) {
                // is older than the parameter threshold δ. ""delete""
                deleted = oldest;
            } else {
                // can't delete any, so merge
                Cluster *merged = NULL;
                minDist = 0;
                for (size_t aId = 0; aId < params->cluStream_q_maxMicroClusters - 1; aId++) {
                    Cluster *a = &microClusters[aId];
                    for (size_t bId = aId + 1; bId < params->cluStream_q_maxMicroClusters; bId++) {
                        Cluster *b = &microClusters[bId];
                        double dist = euclideanDistance(params->dim, a->center, b->center);
                        if (merged == NULL || deleted == NULL || dist < minDist) {
                            merged = a;
                            deleted = b;
                            minDist = dist;
                        }
                    }
                }
                // merged->ids append nearB->ids
                merged->n_matches += deleted->n_matches;
                merged->timeLinearSum += deleted->timeLinearSum;
                merged->timeSquareSum += deleted->timeSquareSum;
                merged->radius = 0;
                merged->distanceAvg = 0;
                for (size_t d = 0; d < params->dim; d++) {
                    merged->ls_valLinearSum[d] += deleted->ls_valLinearSum[d];
                    merged->ss_valSquareSum[d] += deleted->ss_valSquareSum[d];
                    //
                    merged->center[d] = merged->ls_valLinearSum[d] / merged->n_matches;
                    //
                    merged->distanceAvg += merged->ls_valLinearSum[d] / merged->n_matches;
                    //
                    double variance = merged->ls_valLinearSum[d] / merged->n_matches;
                    variance *= variance;
                    variance -= merged->ss_valSquareSum[d] / merged->n_matches;
                    if (variance < 0) {
                        variance = -variance;
                    }
                    merged->radius += variance;
                }
                merged->radius = merged->distanceAvg + sqrt(merged->radius) * params->radiusF;
                //
                merged->time_mu_μ = merged->timeLinearSum / merged->n_matches;
                merged->time_sigma_σ = sqrt((merged->timeSquareSum / merged->n_matches) - (merged->time_mu_μ * merged->time_mu_μ));
            }
            nearest = NULL;
            double minDist;
            for (size_t clId = 0; clId < params->cluStream_q_maxMicroClusters; clId++) {
                double dist = euclideanDistance(params->dim, microClusters[clId].center, oldest->center);
                if (nearest == NULL || minDist > dist) {
                    minDist = dist;
                    nearest = &microClusters[clId];
                }
            }
            deleted->id = nextClusterId++;
            for (size_t d = 0; d < params->dim; d++) {
                deleted->center[d] = ex->val[d];
                deleted->ls_valLinearSum[d] = ex->val[d];
                deleted->ss_valSquareSum[d] = ex->val[d] * ex->val[d];
                deleted->radius = minDist;
                deleted->distanceAvg = 0.0;
                deleted->distanceLinearSum = 0.0;
                deleted->distanceSquareSum = 0.0;
                deleted->distanceStdDev = 0.0;
            }
        }
        // don't use any snapshot
        /*
        if (i % params->clustream_alpha == 0) {
            store away
                the current set of micro-clusters (possibly on disk) to-
                gether with their id list, and indexed by their time of
                storage. We also delete the least recent snapshot
        }
        */
    }
    fprintf(stderr, "CluStream final cluster %lu\n", nextClusterId);
    // macro clustering over all data
    Cluster *clusters = calloc(params->k, sizeof(Cluster));
    for (size_t clId = 0; clId < params->k; clId++) {
        clusters[clId].id = initalId + clId;
        clusters[clId].n_matches = 0;
        clusters[clId].center = calloc(params->dim, sizeof(double));
        clusters[clId].ls_valLinearSum = calloc(params->dim, sizeof(double));
        clusters[clId].ss_valSquareSum = calloc(params->dim, sizeof(double));
        for (size_t d = 0; d < params->dim; d++) {
            clusters[clId].center[d] = microClusters[clId].ls_valLinearSum[d] / microClusters[clId].n_matches;
            clusters[clId].ls_valLinearSum[d] = 0.0;
            clusters[clId].ss_valSquareSum[d] = 0.0;
        }
    }
    Example *pseudoExamples = calloc(params->cluStream_q_maxMicroClusters, sizeof(Example));
    for (size_t clId = 0; clId < params->cluStream_q_maxMicroClusters; clId++) {
        pseudoExamples[clId].id = clId;
        pseudoExamples[clId].val = calloc(params->dim, sizeof(double));
        for (size_t d = 0; d < params->dim; d++) {
            pseudoExamples[clId].val[d] = microClusters[clId].ls_valLinearSum[d] / microClusters[clId].n_matches;
        }
    }
    kMeans(params, clusters, pseudoExamples, params->cluStream_q_maxMicroClusters);
    printTiming(trainingSetSize);
    return clusters;
}

Cluster* clustering(Params *params, Example trainingSet[], unsigned int trainingSetSize, unsigned int initalId) {
    Cluster *clusters;
    if (params->useCluStream) {
        clusters = cluStream(params, trainingSet, trainingSetSize, initalId);
    } else {
        clusters = kMeansInit(params, trainingSet, trainingSetSize, initalId);
        kMeans(params, clusters, trainingSet, trainingSetSize);
    }
    //
    double *distances = calloc(trainingSetSize, sizeof(double));
    Cluster **n_matches = calloc(trainingSetSize, sizeof(Cluster *));
    for (size_t k = 0; k < params->k; k++) {
        clusters[k].radius = 0.0;
        clusters[k].n_matches = 0;
        clusters[k].distanceLinearSum = 0.0;
        clusters[k].distanceSquareSum = 0.0;
    }
    for (size_t i = 0; i < trainingSetSize; i++) {
        double minDist = params->dim * 2;
        Cluster *nearest = NULL;
        for (size_t k = 0; k < params->k; k++) {
            double dist = euclideanDistance(params->dim, clusters[k].center, trainingSet[i].val);
            if (nearest == NULL || dist <= minDist) {
                minDist = dist;
                nearest = &clusters[k];
            }
        }
        distances[i] = minDist;
        n_matches[i] = nearest;
        //
        nearest->n_matches++;
        nearest->distanceLinearSum += minDist;
        nearest->distanceSquareSum += minDist * minDist;
        for (size_t d = 0; d < params->dim; d++) {
            nearest->ls_valLinearSum[d] += trainingSet[i].val[d];
            nearest->ss_valSquareSum[d] += trainingSet[i].val[d] * trainingSet[i].val[d];
        }
    }
    for (size_t k = 0; k < params->k; k++) {
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
        clusters[k].radius = clusters[k].distanceAvg + params->radiusF * clusters[k].distanceStdDev;
    }
    return clusters;
}

char *printableLabel(char label) {
    if (isalnum(label) || label == '-') {
        char *ret = calloc(2, sizeof(char));
        ret[0] = label;
        return ret;
    }
    char *ret = calloc(20, sizeof(char));
    sprintf(ret, "%d", label);
    return ret;
}

Model *training(Params *params) {
    clock_t start = clock();
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
    while (1) {
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
        assertDiffer(nClasses, 254);
        classesSize[l]++;
        trainingSetByClass[l] = realloc(trainingSetByClass[l], classesSize[l] * sizeof(Example));
        Example *ex = &trainingSetByClass[l][classesSize[l] -1];
        //
        ex->id = id;
        ex->val = value;
        ex->class = class;
        id++;
        // if (id > 71990) fprintf(stderr, "Ex(id=%d, val=%le, class=%c)\n", ex->id, ex->val[0], ex->class);
        //
        int hasEmptyline;
        scanf("\n%n", &hasEmptyline);
        if (hasEmptyline == 2) break;
    }
    //
    fprintf(stderr, "Training %u examples with %d classes (%s)\n", id, nClasses, classes);
    Model *model = calloc(1, sizeof(Model));
    model->size = 0;
    model->nextLabel = '\0';
    model->clusters = calloc(1, sizeof(Cluster));
    FILE *modelFile;
    if (params->useCluStream) {
        modelFile = fopen("out/baseline-CluStream-models/baseline_0.csv", "w");
    } else {
        modelFile = fopen("out/baseline-models/baseline_0.csv", "w");
    }
    fprintf(modelFile, "#id, label, n_matches, distanceAvg, distanceStdDev, radius");
    //, ls_valLinearSum, ss_valSquareSum, distanceLinearSum, distanceSquareSum\n");
    for (unsigned int d = 0; d < params->dim; d++)
        fprintf(modelFile, ", c%u", d);
    fprintf(modelFile, "\n");
    for (size_t l = 0; l < nClasses; l++) {
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
            //
            fprintf(modelFile, "%10u, %s, %10u, %le, %le, %le",
                    clusters[k].id, printableLabel(clusters[k].label), clusters[k].n_matches,
                    clusters[k].distanceAvg, clusters[k].distanceStdDev, clusters[k].radius);
            for (unsigned int d = 0; d < params->dim; d++)
                fprintf(modelFile, ", %le", clusters[k].center[d]);
            fprintf(modelFile, "\n");
        }
        free(clusters);
    }
    fclose(modelFile);
    printTiming(id);
    return model;
}

#define UNK_LABEL '-'

Match *identify(Params *params, Model *model, Example *example, Match *match) {
    // Match *match = calloc(1, sizeof(Match));
    match->label = UNK_LABEL;
    match->cluster = NULL;
    for (size_t k = 0; k < model->size; k++) {
        double dist = euclideanDistance(params->dim, example->val, model->clusters[k].center);
        if (match->cluster == NULL || dist <= match->distance) {
            match->distance = dist;
            match->cluster = &model->clusters[k];
        }
    }
    assertDiffer(match->cluster, NULL);
    if (match->distance <= match->cluster->radius) {
        match->label = match->cluster->label;
    }
    return match;
}

void noveltyDetection(Params *params, Model *model, Example *unknowns, size_t unknownsSize) {
    clock_t start = clock();
    char fileName[255];
    sprintf(fileName, "out/baseline-%smodels/baseline_%d.csv", params->useCluStream ? "CluStream-" : "", model->size);
    FILE *modelFile = fopen(fileName, "w");
    fprintf(modelFile, "#id, label, n_matches, distanceAvg, distanceStdDev, radius");
    //, ls_valLinearSum, ss_valSquareSum, distanceLinearSum, distanceSquareSum\n");
    for (unsigned int d = 0; d < params->dim; d++)
        fprintf(modelFile, ", c%u", d);
    fprintf(modelFile, "\n");
    //
    Cluster *clusters = clustering(params, unknowns, unknownsSize, model->size);
    for (size_t k = 0; k < params->k; k++) {
        if (clusters[k].n_matches < params->minExamplesPerCluster) continue;
        //
        double minDist;
        Cluster *nearest = NULL;
        for (size_t i = 0; i < model->size; i++) {
            double dist = euclideanDistance(params->dim, clusters[k].center, model->clusters[i].center);
            if (nearest == NULL || dist < minDist) {
                minDist = dist;
                nearest = &model->clusters[i];
            }
        }
        if (minDist <= nearest->distanceAvg + params->noveltyF * nearest->distanceStdDev) {
            clusters[k].label = nearest->label;
        } else {
            clusters[k].label = model->nextLabel;
            // inc label
            model->nextLabel = (model->nextLabel + 1) % 255;
            // fprintf(stderr, "Novelty %s\n", printableLabel(clusters[k].label));
        }
        //
        clusters[k].id = model->size;
        model->size++;
        model->clusters = realloc(model->clusters, model->size * sizeof(Cluster));
        model->clusters[model->size - 1] = clusters[k];
        //
        fprintf(modelFile, "%10u, %s, %10u, %le, %le, %le",
                clusters[k].id, printableLabel(clusters[k].label), clusters[k].n_matches,
                clusters[k].distanceAvg, clusters[k].distanceStdDev, clusters[k].radius);
        for (unsigned int d = 0; d < params->dim; d++)
            fprintf(modelFile, ", %le", clusters[k].center[d]);
        fprintf(modelFile, "\n");
    }
    free(clusters);
    fclose(modelFile);
    printTiming((int) unknownsSize);
}

void minasOnline(Params *params, Model *model) {
    clock_t start = clock();
    unsigned int id = 0;
    Match match;
    Example example;
    example.val = calloc(params->dim, sizeof(double));
    printf("#pointId,label\n");
    size_t unknownsMaxSize = params->minExamplesPerCluster * params->k;
    size_t noveltyDetectionTrigger = params->minExamplesPerCluster * params->k;
    Example *unknowns = calloc(unknownsMaxSize, sizeof(Example));
    size_t unknownsSize = 0;
    size_t lastNDCheck = 0;
    int hasEmptyline = 0;
    while (!feof(stdin) && hasEmptyline != 2) {
        for (size_t d = 0; d < params->dim; d++) {
            assertEquals(scanf("%lf,", &example.val[d]), 1);
        }
        // ignore class
        char class;
        assertEquals(scanf("%c", &class), 1);
        example.id = id;
        id++;
        scanf("\n%n", &hasEmptyline);
        //
        identify(params, model, &example, &match);
        printf("%10u,%s\n", example.id, printableLabel(match.label));
        //
        if (match.label != UNK_LABEL) continue;
        unknowns[unknownsSize] = example;
        unknowns[unknownsSize].val = calloc(params->dim, sizeof(double));
        for (size_t d = 0; d < params->dim; d++) {
            unknowns[unknownsSize].val[d] = example.val[d];
        }
        unknownsSize++;
        if (unknownsSize >= unknownsMaxSize) {
            unknownsMaxSize *= 2;
            unknowns = realloc(unknowns, unknownsMaxSize * sizeof(Example));
        }
        //
        if (unknownsSize % noveltyDetectionTrigger == 0 && id - lastNDCheck > noveltyDetectionTrigger) {
            lastNDCheck = id;
            noveltyDetection(params, model, unknowns, unknownsSize);
            //
            size_t reclassified = 0;
            for (size_t ex = 0; ex < unknownsSize; ex++) {
                identify(params, model, &unknowns[ex], &match);
                // compress
                unknowns[ex - reclassified] = unknowns[ex];
                if (match.label == UNK_LABEL)
                    continue;
                printf("%10u,%s\n", unknowns[ex].id, printableLabel(match.label));
                reclassified++;
            }
            fprintf(stderr, "Reclassified %lu\n", reclassified);
            unknownsSize -= reclassified;
        }
    }
    // final flush
    noveltyDetection(params, model, unknowns, unknownsSize);
    size_t reclassified = 0;
    for (size_t ex = 0; ex < unknownsSize; ex++) {
        identify(params, model, &unknowns[ex], &match);
        // compress
        unknowns[ex - reclassified] = unknowns[ex];
        if (match.label == UNK_LABEL)
            continue;
        printf("%10u,%s\n", unknowns[ex].id, printableLabel(match.label));
        reclassified++;
    }
    fprintf(stderr, "Final flush %lu\n", reclassified);
    printTiming(id);
    //
    FILE *modelFile;
    if (params->useCluStream) {
        modelFile = fopen("out/baseline-CluStream-models/baseline_final.csv", "w");
    } else {
        modelFile = fopen("out/baseline-models/baseline_final.csv", "w");
    }
    fprintf(modelFile, "#id, label, n_matches, distanceAvg, distanceStdDev, radius");
    //, ls_valLinearSum, ss_valSquareSum, distanceLinearSum, distanceSquareSum\n");
    for (unsigned int d = 0; d < params->dim; d++)
        fprintf(modelFile, ", c%u", d);
    fprintf(modelFile, "\n");
    for (size_t k = 0; k < model->size; k++) {
        Cluster *cl = &model->clusters[k];
        fprintf(modelFile, "%10u, %s, %10u, %le, %le, %le",
                cl->id, printableLabel(cl->label), cl->n_matches,
                cl->distanceAvg, cl->distanceStdDev, cl->radius);
        for (unsigned int d = 0; d < params->dim; d++)
            fprintf(modelFile, ", %le", cl->center[d]);
        fprintf(modelFile, "\n");
    }
    fclose(modelFile);
}

int main(int argc, char const *argv[]) {
    if (argc == 2) {
        fprintf(stderr, "reading from file %s\n", argv[1]);
        stdin = fopen(argv[1], "r");
    }
    Params params;
    params.executable = argv[0];
    fprintf(stderr, "%s\n", params.executable);
    assertEquals(scanf("k=%d\n", &params.k), 1);
    assertEquals(scanf("dim=%d\n", &params.dim), 1);
    assertEquals(scanf("precision=%lf\n", &params.precision), 1);
    assertEquals(scanf("radiusF=%lf\n", &params.radiusF), 1);
    assertEquals(scanf("minExamplesPerCluster=%u\n", &params.minExamplesPerCluster), 1);
    assertEquals(scanf("noveltyF=%lf\n", &params.noveltyF), 1);
    assertEquals(scanf("useCluStream=%u\n", &params.useCluStream), 1);
    assertEquals(scanf("cluStream_q_maxMicroClusters=%u\n", &params.cluStream_q_maxMicroClusters), 1);
    assertEquals(scanf("cluStream_time_threshold_delta_δ=%lf\n", &params.cluStream_time_threshold_delta_δ), 1);
    fprintf(stderr, "\tk = %d\n", params.k);
    fprintf(stderr, "\tdim = %d\n", params.dim);
    fprintf(stderr, "\tprecision = %le\n", params.precision);
    fprintf(stderr, "\tradiusF = %le\n", params.radiusF);
    fprintf(stderr, "\tminExamplesPerCluster = %u\n", params.minExamplesPerCluster);
    fprintf(stderr, "\tnoveltyF = %lf\n", params.noveltyF);
    fprintf(stderr, "\tuseCluStream = %u\n", params.useCluStream);
    fprintf(stderr, "\tcluStream_q_maxMicroClusters = %u\n", params.cluStream_q_maxMicroClusters);
    fprintf(stderr, "\tcluStream_time_threshold_delta_δ = %lf\n", params.cluStream_time_threshold_delta_δ);

    Model *model = training(&params);

    minasOnline(&params, model);

    return EXIT_SUCCESS;
}
