#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <mpi.h>

#include "./base.h"

int main(int argc, char const *argv[]) {
    clock_t start = clock();
    int kParam = 100, dim = 22, minExamplesPerCluster = 20;
    double precision = 1.0e-08, radiusF = 0.10, noveltyF = 2.0;
    //
    kParam=100; dim=22; precision=1.0e-08; radiusF=0.25; minExamplesPerCluster=20; noveltyF=1.4;
    // kParam=100; dim=22; precision=1.0e-08; radiusF=0.10; minExamplesPerCluster=20; noveltyF=2.0;
    //
    fprintf(stderr, "%s; kParam=%d; dim=%d; precision=%le; radiusF=%le; minExamplesPerCluster=%d; noveltyF=%le\n", argv[0], PARAMS);
    Model *model = calloc(1, sizeof(Model));
    model->size = 0;
    model->nextLabel = '\0';
    model->clusters = calloc(kParam, sizeof(Cluster));
    unsigned int id = 0;
    // Match match;
    Example example;
    example.val = calloc(dim, sizeof(double));
    // printf("#pointId,label\n");
    size_t unknownsMaxSize = minExamplesPerCluster * kParam;
    size_t noveltyDetectionTrigger = minExamplesPerCluster * kParam;
    Example *unknowns = calloc(unknownsMaxSize, sizeof(Example));
    size_t unknownsSize = 0;
    size_t lastNDCheck = 0;
    int hasEmptyline = 0;
    fprintf(stderr, "Taking unknown stream from stdin\n");
    char *lineptr = NULL;
    size_t n = 0, inputLine = 0;
    ssize_t nread;
    while (!feof(stdin) && hasEmptyline != 2) {
        nread = getline(&lineptr, &n, stdin);
        inputLine++;
        if (lineptr[0] == 'C') {
            // fprintf(stderr, "wtf '%s'.\n", lineptr);
            addClusterLine(kParam, dim, model, lineptr);
            printCluster(dim, &model->clusters[model->size -1]);
            continue;
        }
        // id++;
        if (lineptr[0] != 'U') {
            continue;
        }
        // line is on format "Unknown: %10u" + dim * ", %le"
        int readCur = 0, readTot = 0;
        assert(sscanf(lineptr, "Unknown: %10u%n", &example.id, &readCur));
        readTot += readCur;
        for (size_t d = 0; d < dim; d++) {
            assertMsg(sscanf(&lineptr[readTot], ", %le%n", &example.val[d], &readCur), "Didn't understand '%s'.", &lineptr[readTot]);
            readTot += readCur;
        }
        id = example.id > id ? example.id : id;
        unknowns[unknownsSize] = example;
        unknownsSize++;
        example.val = calloc(dim, sizeof(double));
        if (unknownsSize >= unknownsMaxSize) {
            unknownsMaxSize *= 2;
            unknowns = realloc(unknowns, unknownsMaxSize * sizeof(Example));
        }
        //
        if (model->size < kParam) {
            continue;
        }
        if (unknownsSize % noveltyDetectionTrigger == 0 && id - lastNDCheck > noveltyDetectionTrigger) {
            lastNDCheck = id;
            unsigned int prevSize = model->size;
            noveltyDetection(PARAMS, model, unknowns, unknownsSize);
            unsigned int nNewClusters = model->size - prevSize;
            //
            size_t reclassified = 0;
            for (size_t ex = 0; ex < unknownsSize; ex++) {
                // compress
                unknowns[ex - reclassified] = unknowns[ex];
                Cluster *nearest;
                double distance = nearestClusterVal(dim, &model->clusters[prevSize], nNewClusters, unknowns[ex].val, &nearest);
                assert(nearest != NULL);
                if (distance <= nearest->distanceMax) {
                    reclassified++;
                }
            }
            fprintf(stderr, "Reclassified %lu\n", reclassified);
            unknownsSize -= reclassified;
            //
            for (size_t k = prevSize; k < model->size; k++) {
                printCluster(dim, &model->clusters[k]);
            }
        }
    }
    fprintf(stderr, "Final flush %lu\n", unknownsSize);
    // final flush
    if (model->size >= kParam && unknownsSize > kParam) {
        unsigned int prevSize = model->size;
        noveltyDetection(PARAMS, model, unknowns, unknownsSize);
        unsigned int nNewClusters = model->size - prevSize;
        //
        size_t reclassified = 0;
        for (size_t ex = 0; ex < unknownsSize; ex++) {
            // compress
            unknowns[ex - reclassified] = unknowns[ex];
            Cluster *nearest;
            double distance = nearestClusterVal(dim, &model->clusters[prevSize], nNewClusters, unknowns[ex].val, &nearest);
            assert(nearest != NULL);
            if (distance <= nearest->distanceMax) {
                // printf("%10u,%s\n", unknowns[ex].id, printableLabel(nearest->label));
                reclassified++;
            }
        }
        fprintf(stderr, "Reclassified %lu\n", reclassified);
        unknownsSize -= reclassified;
        //
        for (size_t k = prevSize; k < model->size; k++) {
            printCluster(dim, &model->clusters[k]);
        }
    }
    free(model);
    fflush(stdout);
    fprintf(stderr, "[%s] %le seconds. At %s:%d\n", argv[0], ((double)clock() - start) / 1000000.0, __FILE__, __LINE__);
    return EXIT_SUCCESS;
}
