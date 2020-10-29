#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#define USE_MPI 1

#include "../base/base.h"
#include "../base/minas.h"
#include "../modules/modules.h"
#include "../mpi/mfog-mpi.h"

/* Used as argument to thread_start() */
typedef struct {
    pthread_t thread_id;        /* ID returned by pthread_create() */
    int mutex;
    Model *model;
    Params *params;
} thread_info;

static void *sampler(void *arg) {
    thread_info *tinfo = arg;
    Model *model = tinfo->model;
    Params *params = tinfo->params;
    //
    clock_t start = clock();
    unsigned int id = 0;
    Example *example;
    Match *match;
    int exampleBufferSize;
    char *exampleBuffer;
    double *valuePtr;
    if (params->mpiRank == 0) {
        exampleBufferSize = sizeof(Example) + params->dim * sizeof(double);
        exampleBuffer = malloc(exampleBufferSize);
        MPI_Bcast(&exampleBufferSize, 1, MPI_INT, MFOG_MAIN_RANK, MPI_COMM_WORLD);
    } else {
        MPI_Bcast(&exampleBufferSize, 1, MPI_INT, MFOG_MAIN_RANK, MPI_COMM_WORLD);
        exampleBuffer = malloc(exampleBufferSize);
        valuePtr = calloc(params->dim + 1, sizeof(double));
        example = calloc(1, sizeof(Example));
        match = calloc(1, sizeof(Match));
    }
    int hasEmptyline = 0, dest;
    fprintf(stderr, "Taking test stream from stdin\n");
    while (!feof(stdin) && hasEmptyline != 2) {
        for (size_t d = 0; d < params->dim; d++) {
            assert(scanf("%lf,", &example->val[d]) == 1);
        }
        // ignore class
        char class;
        assert(scanf("%c", &class) == 1);
        example->id = id;
        id++;
        scanf("\n%n", &hasEmptyline);
        //
        // tradeExample(params, example, exampleBuffer, exampleBufferSize, &dest, valuePtr);
        int position = 0;
        MPI_Pack(example, sizeof(Example), MPI_BYTE, exampleBuffer, exampleBufferSize, &position, MPI_COMM_WORLD);
        MPI_Pack(example->val, params->dim, MPI_DOUBLE, exampleBuffer, exampleBufferSize, &position, MPI_COMM_WORLD);
        MPI_Send(exampleBuffer, position, MPI_PACKED, dest, 2004, MPI_COMM_WORLD);
        dest = (dest + 1) % params->mpiSize;
    }
    printTiming(sampler, id);
    pthread_exit(NULL);
}

static void *modelCommEntryFunc(void *arg) {
    thread_info *tinfo = arg;
    // Model *model = tinfo->model;
    Params *params = tinfo->params;
    //
    clock_t start = clock();
    size_t traded = 0;
    while (1) {
        // trade model
    }
    printTiming(modelCommEntryFunc, traded);
    pthread_exit(NULL);
}

static void *noveltyDetectionEntryFunc(void *arg) {
    thread_info *tinfo = arg;
    // Model *model = tinfo->model;
    Params *params = tinfo->params;
    //
    clock_t start = clock();
    size_t traded = 0;
    while (1) {
        // trade model
    }
    printTiming(noveltyDetectionEntryFunc, traded);
    pthread_exit(NULL);
}

int main(int argc, char const *argv[], char *env[]) {
    clock_t start = clock();
    Params *params = setup(argc, argv, env);
    //
    assertMsg(params->useInlineND, "Must use useInlineND, got %d.", params->useInlineND);
    assertMsg(params->useMPI, "Must use MPI, got %d.", params->useMPI);

    Model *model = calloc(1, sizeof(Model));

    model->size = 0;
    model->clusters = calloc(params->k, sizeof(Cluster));
    if (params->mpiRank == MFOG_MAIN_RANK) {
        int lines = 0, emtpyLines = 0;
        char *line = NULL;
        size_t len = 0;
        int read;
        do {
            read = getline(&line, &len, stdin);
            // if (read == -1) break;
            // assert(read != -1);
            // fprintf(stderr, "Retrieved line of length %d / %lu:\n'%s'\n", read, len, line);
            lines++;
            if (read == 1 && line[0] == '\n'){
                emtpyLines++;
            }
            if (read > 0 && line[0] != '#') {
                appendClusterFromStore(params, line, read, model);
            }
        } while (read != 0 && emtpyLines < 1);
        free(line);
        fprintf(stderr, "model->size = %u from %d lines.\n", model->size, lines);
    }
    tradeModel(params, model);
    fprintf(stderr, "%d@%s model->size = %u.\n", params->mpiRank, params->mpiHostname, model->size);

    /**
     * examples -> classify -> match -> ND
     * init model ->  î  <-  model  <- |
     **/
    pthread_attr_t attr;
    int s = pthread_attr_init(&attr);
    assertMsg(s == 0, "pthread_attr_init %d.", s);
    //
    thread_info classifierTinfo, modelCommTinfo, noveltyDetectionTinfo;
    classifierTinfo.model = modelCommTinfo.model = noveltyDetectionTinfo.model = model;
    classifierTinfo.params = modelCommTinfo.params = noveltyDetectionTinfo.params = params;
    //
    pthread_create(&classifierTinfo.thread_id, &attr, &classifierEntryFunc, &classifierTinfo);
    pthread_create(&modelCommTinfo.thread_id, &attr, &modelCommEntryFunc, &modelCommTinfo);
    pthread_create(&noveltyDetectionTinfo.thread_id, &attr, &noveltyDetectionEntryFunc, &noveltyDetectionTinfo);
    
    s = pthread_attr_destroy(&attr);
    assertMsg(s != 0, "pthread_attr_destroy %d", s);
    
    thread_info tinfo[3] = { classifierTinfo, modelCommTinfo, noveltyDetectionTinfo };
    for (size_t i = 0; i < 3; i++) {
        void *res;
        s = pthread_join(tinfo[i].thread_id, &res);
        assertMsg(s != 0, "pthread_attr_destroy %d", s);

        printf("Joined with thread %lu; returned value was %d\n", i, *(int *)res);
        free(res);      /* Free memory allocated by thread */
    }

    tearDown(argc, argv, env, params);
    printTiming(main, 1);
    printTimeLog(params);
    return EXIT_SUCCESS;
}
