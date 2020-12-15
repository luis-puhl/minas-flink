#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <sys/select.h>

#include "../base.h"

int main(int argc, char const *argv[]) {
    clock_t start = clock();
    unsigned int kParam = 100, dim = 22, minExamplesPerCluster = 20, thresholdForgettingPast = 10000;
    double precision = 1.0e-08, radiusF = 0.25, noveltyF = 1.4;
    fprintf(stderr, "%s; kParam=%u; dim=%u; precision=%le; radiusF=%le; minExamplesPerCluster=%u; noveltyF=%le, thresholdForgettingPast=%u\n", argv[0], PARAMS);
    //
    // assertMsg(argc == 2, "Expected model files as argument, got %d\n", argc -1);
    FILE *modelFile = stdin;
    if (argc == 2) {
        fprintf(stderr, "Taking model from '%s'\n", argv[1]);
        modelFile = fopen(argv[1], "r");
    }
    fd_set fds;
    FD_ZERO(&fds);
    int inFd = fileno(stdin), modelFd = fileno(modelFile);
    int maxFD = inFd > modelFd ? inFd : modelFd;
    FD_SET(inFd, &fds);
    FD_SET(modelFd, &fds);
    //
    Model *model = calloc(1, sizeof(Model));
    model->size = 0;
    model->nextLabel = 0;
    model->clusters = calloc(kParam, sizeof(Cluster));
    unsigned long id = 0;
    Match match;
    Example example;
    example.val = calloc(dim, sizeof(double));
    Cluster cluster;
    cluster.center = calloc(dim, sizeof(double));
    printf("#pointId,label\n");
    //
    char *lineptr = NULL;
    size_t n = 0, inputLine = 0;
    while (model->size < kParam) {
        char lineType = getMfogLine(modelFile, &lineptr, &n, kParam, dim, &id, model, &cluster, &example);
        inputLine++;
        assertMsg(lineType == 'C', "Expected cluster and got %c", lineType);
        if (model->size <= cluster.id) {
            addCluster(kParam, dim, &cluster, model);
            printCluster(dim, &cluster);
            if (model->size == kParam) {
                fprintf(stderr, "[%s] model complete\n", argv[0]);
            }
        }
    }
    fprintf(stderr, "taking test from stdin\n");
    while (!feof(stdin)) {
        int retval = select(maxFD + 1, &fds, NULL, NULL, NULL);
        if (retval <= 0) {
            continue;
        }
        if (modelFile != stdin && FD_ISSET(modelFd, &fds) && !feof(modelFile) && !ferror(modelFile)) {
            char lineType = getMfogLine(modelFile, &lineptr, &n, kParam, dim, &id, model, &cluster, &example);
            inputLine++;
            assertMsg(lineType == 'C', "Expected cluster 'C' and got '%s'", lineptr);
            if (model->size <= cluster.id) {
                addCluster(kParam, dim, &cluster, model);
                printCluster(dim, &cluster);
            }
        }
        if (!FD_ISSET(inFd, &fds) || feof(stdin) || ferror(stdin)) {
            continue;
        }
        char lineType = getMfogLine(stdin, &lineptr, &n, kParam, dim, &id, model, &cluster, &example);
        inputLine++;
        if (lineType == 'C') {
            if (model->size <= cluster.id) {
                addCluster(kParam, dim, &cluster, model);
                printCluster(dim, &cluster);
            }
            if (model->size == kParam) {
                fprintf(stderr, "model complete\n");
            }
        }
        if (lineType != 'E') {
            continue;
        }
        if (model->size < kParam) {
            continue;
        }
        //
        identify(kParam, dim, precision, radiusF, model, &example, &match, thresholdForgettingPast);
        printf("%10u,%s\n", example.id, printableLabel(match.label));
        fflush(stdout);
        //
        if (match.label == MINAS_UNK_LABEL) {
            printf("Unknown: %10u", example.id);
            for (unsigned int d = 0; d < dim; d++) {
                printf(", %le", example.val[d]);
            }
            printf("\n");
            fflush(stdout);
        }
    }
    char *stats = calloc(model->size * 30, sizeof(char));
    fprintf(stderr, "[%s] Statistics %s\n", argv[0], labelMatchStatistics(model, stats));
    free(stats);
    free(model);
    fprintf(stderr, "[%s] %le seconds. At %s:%d\n", argv[0], ((double)clock() - start) / 1000000.0, __FILE__, __LINE__);
    return 0;
}
