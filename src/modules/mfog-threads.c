#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#define USE_MPI 0

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

static void *classifierEntryFunc(void *arg) {
    thread_info *tinfo = arg;
    Model *model = tinfo->model;
    Params *params = tinfo->params;
    //
    clock_t start = clock();
    unsigned int id = 0;
    Match match;
    Example example;
    example.val = calloc(params->dim, sizeof(double));
    printf("#pointId,label\n");
    int hasEmptyline = 0;
    fprintf(stderr, "Taking test stream from stdin\n");
    while (!feof(stdin) && hasEmptyline != 2) {
        for (size_t d = 0; d < params->dim; d++) {
            assert(scanf("%lf,", &example.val[d]) == 1);
        }
        // ignore class
        char class;
        assert(scanf("%c", &class) == 1);
        example.id = id;
        id++;
        scanf("\n%n", &hasEmptyline);
        //
        identify(params, model, &example, &match);
        printf("%10u,%s\n", example.id, printableLabel(match.label));
        //
        if (match.label != UNK_LABEL) continue;
    }
    printTiming(classifierEntryFunc, id);
    return EXIT_SUCCESS;
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
    return EXIT_SUCCESS;
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
    return EXIT_SUCCESS;
}

int main(int argc, char const *argv[], char *env[]) {
    clock_t start = clock();
    Params *params = setup(argc, argv, env);
    //
    int s;
    pthread_attr_t attr;

    s = pthread_attr_init(&attr);
    if (s != 0)
        errx(s, "pthread_attr_init. At "__FILE__":%d\n", __LINE__);
    assertMsg(params->useInlineND, "Must use useInlineND, got %d.", params->useInlineND);

    Model *model = calloc(1, sizeof(Model));
    model->size = 0;
    model->clusters = calloc(params->k, sizeof(Cluster));
    if (params->mpiRank == MFOG_MAIN_RANK) {
        fprintf(stderr, "useInlineND\n");
        int lines = 0;
        char *line = NULL;
        size_t len = 0;
        int read;
        while (1) {
            read = getline(&line, &len, stdin);
            assert(read != -1);
            // fprintf(stderr, "Retrieved line of length %d / %lu:\n'%s'\n", read, len, line);
            lines++;
            if (read == 0 || read == 1 || line[0] == '\n') break;
            if (line[0] != '#') {
                appendClusterFromStore(params, line, read, model);
            }
        }
        free(line);
        fprintf(stderr, "model->size = %u from %d lines.\n", model->size, lines);
    }

    thread_info classifierTinfo, modelCommTinfo, noveltyDetectionTinfo;
    /**
     * examples -> classify -> match -> ND
     * init model ->  î  <-  model  <- |
     **/
    pthread_create(&classifierTinfo.thread_id, &attr, &classifierEntryFunc, &classifierTinfo);
    pthread_create(&modelCommTinfo.thread_id, &attr, &modelCommEntryFunc, &modelCommTinfo);
    pthread_create(&noveltyDetectionTinfo.thread_id, &attr, &noveltyDetectionEntryFunc, &noveltyDetectionTinfo);
    
    s = pthread_attr_destroy(&attr);
    if (s != 0)
        errx(s, "pthread_attr_destroy");
    
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
