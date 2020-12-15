#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <mpi.h>

#include "../base.h"

struct LinkedArrayExs{
    Example *examples;
    size_t size;
    struct LinkedArrayExs *next;
};

#define MPI_MAIN_RANK 0
#define MPI_TAG_CLUSTER 2000
#define MPI_TAG_UNKNOWN 2001

int main(int argc, char const *argv[]) {
    int mpiReturn = MPI_Init(&argc, (char ***)&argv);
    int mpiSize, mpiRank;
    mpiReturn += MPI_Comm_size(MPI_COMM_WORLD, &mpiSize);
    mpiReturn += MPI_Comm_rank(MPI_COMM_WORLD, &mpiRank);
    if (mpiReturn != MPI_SUCCESS) {
        MPI_Abort(MPI_COMM_WORLD, mpiReturn);
        fail("MPI Abort %d\n", mpiReturn);
    }
    //
    int kParam = 100, dim = 22, minExamplesPerCluster = 20;
    double precision = 1.0e-08, radiusF = 0.25, noveltyF = 1.4;
    if (mpiRank == 0)
        fprintf(stderr, "%s; kParam=%d; dim=%d; precision=%le; radiusF=%le; minExamplesPerCluster=%d; noveltyF=%le\n", argv[0], PARAMS);
    Model *model = calloc(1, sizeof(Model));
    model->size = 0;
    model->nextLabel = 0;
    model->clusters = calloc(kParam, sizeof(Cluster));
    // void minasOnline(PARAMS_ARG, Model *model) {
    unsigned int id = 0;
    Match match;
    Example example;
    example.val = calloc(dim, sizeof(double));
    if (mpiRank == 0)
        printf("#pointId,label\n");
    // size_t unknownsMaxSize = minExamplesPerCluster * kParam;
    // size_t noveltyDetectionTrigger = minExamplesPerCluster * kParam;
    struct LinkedArrayExs *unknownsHead = calloc(1, sizeof(struct LinkedArrayExs));
    struct LinkedArrayExs *unknowns = unknownsHead;
    unknowns->examples = calloc(kParam, sizeof(Example));
    unknowns->size = 0;
    unknowns->next = NULL;
    if (mpiRank == 0)
        fprintf(stderr, "Taking test stream from stdin\n");
    char *lineptr = NULL;
    size_t n = 0, inputLine = 0;
    ssize_t nread = 0;
    while (mpiRank == 0 && !feof(stdin)) {
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
    if (mpiRank != 0) {
        MPI_Status status;
        MPI_Probe(0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        switch (status.MPI_TAG) {
        case MPI_TAG_CLUSTER:
            mpiReturn = MPI_Bcast(buffer, bufferSize, MPI_PACKED, MFOG_MAIN_RANK, MPI_COMM_WORLD);
            MPI_RETURN
            model = malloc(sizeof(Model));
            mpiReturn = MPI_Unpack(buffer, bufferSize, &position, model, sizeof(Model), MPI_BYTE, MPI_COMM_WORLD);
            MPI_RETURN
            model->clusters = malloc(model->size * sizeof(Cluster));
            mpiReturn = MPI_Unpack(buffer, bufferSize, &position, model->clusters, model->size * sizeof(Cluster), MPI_BYTE, MPI_COMM_WORLD);
            MPI_RETURN
            for (int i = 0; i < model->size; i++) {
                model->clusters[i].center = malloc(params->dim * sizeof(double));
                mpiReturn = MPI_Unpack(buffer, bufferSize, &position, model->clusters[i].center, params->dim, MPI_DOUBLE, MPI_COMM_WORLD);
                MPI_RETURN
            }
            break;
        case MPI_TAG_UNKNOWN:
            break;
        default:
            fail("Unknown tag %d", status.MPI_TAG);
            break;
        }
    }
    free(lineptr);
    free(model);
    return EXIT_SUCCESS;
}
