#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <mpi.h>

#include "./base.h"

void minasOnline(PARAMS_ARG, Model *model) {
    unsigned int id = 0;
    Match match;
    Example example;
    example.val = calloc(dim, sizeof(double));
    printf("#pointId,label\n");
    size_t noveltyDetectionTrigger = minExamplesPerCluster * kParam;
    size_t unknownsMaxSize = minExamplesPerCluster * kParam;
    Example *unknowns = calloc(unknownsMaxSize, sizeof(Example));
    size_t unknownsSize = 0;
    size_t lastNDCheck = 0;
    int hasEmptyline = 0;
    fprintf(stderr, "Taking test stream from stdin\n");
    while (!feof(stdin) && hasEmptyline != 2) {
        for (size_t d = 0; d < dim; d++) {
            assert(scanf("%lf,", &example.val[d]));
        }
        // ignore class
        char class;
        assert(scanf("%c", &class));
        example.id = id;
        id++;
        scanf("\n%n", &hasEmptyline);
        //
        identify(kParam, dim, precision, radiusF, model, &example, &match);
        printf("%10u,%s\n", example.id, printableLabel(match.label));
        //
        if (match.label != UNK_LABEL) continue;
        unknowns[unknownsSize] = example;
        unknowns[unknownsSize].val = calloc(dim, sizeof(double));
        for (size_t d = 0; d < dim; d++) {
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
        }
    }
    fprintf(stderr, "Final flush %lu\n", unknownsSize);
    // final flush
    if (unknownsSize > kParam) {
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
                printf("%10u,%s\n", unknowns[ex].id, printableLabel(nearest->label));
                reclassified++;
            }
        }
        fprintf(stderr, "Reclassified %lu\n", reclassified);
        unknownsSize -= reclassified;
    }
}

int main(int argc, char const *argv[]) {
    int kParam = 100, dim = 22, minExamplesPerCluster = 20;
    double precision = 1.0e-08, radiusF = 0.10, noveltyF = 2.0;
    //
    kParam=100; dim=22; precision=1.0e-08; radiusF=0.25; minExamplesPerCluster=20; noveltyF=1.4;
    // kParam=100; dim=22; precision=1.0e-08; radiusF=0.10; minExamplesPerCluster=20; noveltyF=2.0;
    //
    fprintf(stderr, "%s; kParam=%d; dim=%d; precision=%le; radiusF=%le; minExamplesPerCluster=%d; noveltyF=%le\n", argv[0], PARAMS);
    Model *model = training(kParam, dim, precision, radiusF);
    minasOnline(PARAMS, model);
    free(model);
    return EXIT_SUCCESS;
}
