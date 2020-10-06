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

/* Used as argument to thread_start() */
typedef struct {
    pthread_t thread_id;        /* ID returned by pthread_create() */
    int mutex;
    Model *model;
    Params *params;
} thread_info;

static void * classifierEntryFunc(void *arg) {
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
    // size_t unknownsMaxSize = params->minExamplesPerCluster * params->k;
    // size_t noveltyDetectionTrigger = params->minExamplesPerCluster * params->k;
    // Example *unknowns = calloc(unknownsMaxSize, sizeof(Example));
    // size_t unknownsSize = 0;
    // size_t lastNDCheck = 0;
    int hasEmptyline = 0;
    fprintf(stderr, "Taking test stream from stdin\n");
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
        errx(s, "pthread_attr_init");

    thread_info classifierTinfo, modelCommTinfo, noveltyDetectionTinfo;
    // examples -> classify -> match -> ND
    // init model ->  î  <-  model  <- |
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
        if (s != 0)
            errx(s, "pthread_attr_destroy");

        printf("Joined with thread %lu; returned value was %d\n", i, *(int *)res);
        free(res);      /* Free memory allocated by thread */
    }

    tearDown(argc, argv, env, params);
    printTiming(main, 1);
    printTimeLog(params);
    return EXIT_SUCCESS;
}
