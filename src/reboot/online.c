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

struct LinkedArrayExs{
    Example *examples;
    size_t size;
    struct LinkedArrayExs *next;
};

int main(int argc, char const *argv[]) {
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
    // void minasOnline(PARAMS_ARG, Model *model) {
    unsigned int id = 0;
    Match match;
    Example example;
    example.val = calloc(dim, sizeof(double));
    printf("#pointId,label\n");
    // size_t unknownsMaxSize = minExamplesPerCluster * kParam;
    // size_t noveltyDetectionTrigger = minExamplesPerCluster * kParam;
    struct LinkedArrayExs *unknownsHead = calloc(1, sizeof(struct LinkedArrayExs));
    struct LinkedArrayExs *unknowns = unknownsHead;
    unknowns->examples = calloc(kParam, sizeof(Example));
    unknowns->size = 0;
    unknowns->next = NULL;
    // size_t lastNDCheck = 0;
    int hasEmptyline = 0;
    fprintf(stderr, "Taking test stream from stdin\n");
    char *lineptr = NULL;
    size_t n = 0, inputLine = 0;
    ssize_t nread = 0;
    while (!feof(stdin) && hasEmptyline != 2) {
        nread += getline(&lineptr, &n, stdin);
        inputLine++;
        int readCur = 0, readTot = 0;
        // fprintf(stderr,
        //     "line %ld, unk #%ld id %d model %d at byte %ld \t'%.30s'\n",
        //     inputLine, unknowns == NULL ? 0 : unknowns->size, id, model->size, nread, lineptr
        // );
        if (lineptr[0] == 'C') {
            addClusterLine(kParam, dim, model, lineptr);
            if (unknownsHead != NULL && model->size >= kParam) {
                size_t onComplete = 0;
                for (unknowns = unknownsHead; unknowns != NULL;){
                    for (size_t i = 0; i < unknowns->size; i++) {
                        Example *ex = &unknowns->examples[i];
                        identify(kParam, dim, precision, radiusF, model, ex, &match);
                        onComplete++;
                        printf("%10u,%s\n", ex->id, printableLabel(match.label));
                        free(ex->val);
                    }
                    struct LinkedArrayExs *toFree = unknowns;
                    unknowns = unknowns->next;
                    free(toFree);
                }
                unknownsHead = NULL;
                unknowns = NULL;
                fprintf(stderr, "model complete, classified %ld\n", onComplete);
            }
            continue;
        }
        for (size_t d = 0; d < dim; d++) {
            assert(sscanf(&lineptr[readTot], "%lf,%n", &example.val[d], &readCur));
            readTot += readCur;
        }
        // ignore class
        example.id = id;
        id++;
        if (unknowns != NULL && model->size < kParam) {
            // fprintf(stderr, "Unknown: %10u", example.id);
            // for (unsigned int d = 0; d < dim; d++)
            //     fprintf(stderr, ", %le", example.val[d]);
            // fprintf(stderr, "\n");
            // wait until model size is at least k
            if (unknowns->size == kParam) {
                unknowns->next = calloc(1, sizeof(struct LinkedArrayExs));
                unknowns = unknowns->next;
                unknowns->examples = calloc(kParam, sizeof(Example));
                unknowns->size = 0;
                unknowns->next = NULL;
            }
            unknowns->examples[unknowns->size] = example;
            unknowns->examples[unknowns->size].val = calloc(dim, sizeof(double));
            for (unsigned int d = 0; d < dim; d++)
                unknowns->examples[unknowns->size].val[d] = example.val[d];
            unknowns->size++;
            // prepare for next value
            // example.val = calloc(dim, sizeof(double));
            continue;
        }
        identify(kParam, dim, precision, radiusF, model, &example, &match);
        printf("%10u,%s\n", example.id, printableLabel(match.label));
        //
        if (match.label != UNK_LABEL) continue;
        printf("Unknown: %10u", example.id);
        for (unsigned int d = 0; d < dim; d++)
            printf(", %le", example.val[d]);
        printf("\n");
    }
    free(lineptr);
    free(model);
    return EXIT_SUCCESS;
}
