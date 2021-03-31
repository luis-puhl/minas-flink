#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <errno.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <mpi.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>

#include "./base.h"

#define assertMpi(exp)                        \
    if ((mpiReturn = exp) != MPI_SUCCESS) { \
        MPI_Abort(MPI_COMM_WORLD, mpiReturn); \
        errx(EXIT_FAILURE, "MPI Abort %d. At "__FILE__":%d\n", mpiReturn, __LINE__); }
#define MFOG_RANK_MAIN 0
#define MFOG_TAG_EXAMPLE 2004
#define MFOG_TAG_UNKNOWN 2005
#define MFOG_EOS_MARKER '\127'

#define MFOG_OUTPUT_NONE    0
#define MFOG_OUTPUT_MINIMAL 1
#define MFOG_OUTPUT_ALL     2

typedef struct ThreadArgs_st {
    MinasParams *minasParams;
    MinasState *minasState;
    char outputMode, nClassifiers;
    // mpi stuff
    int mpiRank, mpiSize;
    pthread_rwlock_t modelLock;
    sem_t modelReadySemaphore;
} ThreadArgs;

#define printTiming(who)                                                                                              \
    double ioTime_d = ((double)ioTime) / 1000000.0;                                                                   \
    double cpuTime_d = ((double)cpuTime) / 1000000.0;                                                                 \
    double lockTime_d = ((double)lockTime) / 1000000.0;                                                               \
    double totalTime_d = ((double)clock() - start) / 1000000.0;                                                       \
    fprintf(stderr, "[" who " %3d] (items %8lu) (ioTime %le), (cpuTime %le), (lockTime %le), (total %le). At %s:%d\n", \
            args->mpiRank, items, ioTime_d, cpuTime_d, lockTime_d, totalTime_d, __FILE__, __LINE__);

void *classifier(void *arg) {
    clock_t start = clock();
    ThreadArgs *args = arg;
    unsigned int dim = args->minasParams->dim;
    fprintf(stderr, "[classifier %d]\n", args->mpiRank);
    Example example;
    double valuePtr[dim];
    unsigned long items = 0, sends = 0;
    //
    int exampleBufferLen, valueBufferLen;
    int mpiReturn;
    assertMpi(MPI_Pack_size(sizeof(Example), MPI_BYTE, MPI_COMM_WORLD, &exampleBufferLen));
    assertMpi(MPI_Pack_size(dim, MPI_DOUBLE, MPI_COMM_WORLD, &valueBufferLen));
    int bufferSize = exampleBufferLen + valueBufferLen + 2 * MPI_BSEND_OVERHEAD;
    int *buffer = (int *)malloc(bufferSize);
    clock_t ioTime = 0, cpuTime = 0, lockTime = 0;
    while (1) {
        clock_t l0 = clock();
        assertMpi(MPI_Recv(buffer, bufferSize, MPI_PACKED, MPI_ANY_SOURCE, MFOG_TAG_EXAMPLE, MPI_COMM_WORLD, MPI_STATUS_IGNORE));
        int position = 0;
        assertMpi(MPI_Unpack(buffer, bufferSize, &position, &example, sizeof(Example), MPI_BYTE, MPI_COMM_WORLD));
        assertMpi(MPI_Unpack(buffer, bufferSize, &position, valuePtr, dim, MPI_DOUBLE, MPI_COMM_WORLD));
        example.val = valuePtr;
        if (example.label == MFOG_EOS_MARKER) {
            assertMpi(MPI_Send(buffer, position, MPI_PACKED, MFOG_RANK_MAIN, MFOG_TAG_UNKNOWN, MPI_COMM_WORLD));
            break;
        }
        // fprintf(stderr, "[classifier %d] id %u\n", args->mpiRank, example.id);
        clock_t l1 = clock();
        ioTime += (l1 - l0);
        //
        while (args->minasState->model.size < args->minasParams->k) {
            sem_wait(&args->modelReadySemaphore);
            // fprintf(stderr, "[classifier %d] model complete\n", args->mpiRank);
        }
        pthread_rwlock_rdlock(&args->modelLock);
        clock_t l2 = clock();
        lockTime += l2 - l1;
        Match match;
        identify(args->minasParams, args->minasState, &example, &match);
        items++;
        example.label = match.label;
        clock_t l3 = clock();
        cpuTime += (l3 - l2);
        pthread_rwlock_unlock(&args->modelLock);
        //
        position = 0;
        if (example.label == MINAS_UNK_LABEL || match.cluster->isIntrest) {
            assertMpi(MPI_Pack(&example, sizeof(Example), MPI_BYTE, buffer, bufferSize, &position, MPI_COMM_WORLD));
            assertMpi(MPI_Pack(valuePtr, dim, MPI_DOUBLE, buffer, bufferSize, &position, MPI_COMM_WORLD));
            assertMpi(MPI_Send(buffer, position, MPI_PACKED, MFOG_RANK_MAIN, MFOG_TAG_UNKNOWN, MPI_COMM_WORLD));
            sends++;
        }
        clock_t l4 = clock();
        ioTime += (l4 - l3);
    }
    free(buffer);
    fprintf(stderr, "[classifier %d] sends %lu\n", args->mpiRank, sends);
    printTiming("classifier");
    return NULL;
}

/**
 * @brief In rank N != 0, this thread receives model updates.
 * 
 * @param arg ThreadArgs
 * @return void* 
 */
void *m_receiver(void *arg) {
    clock_t start = clock();
    ThreadArgs *args = arg;
    unsigned int dim = args->minasParams->dim;
    fprintf(stderr, "[m_receiver %d]\n", args->mpiRank);
    Cluster *cl = calloc(1, sizeof(Cluster));
    cl->center = calloc(dim, sizeof(double));
    double *valuePtr = cl->center;
    Model *model = &args->minasState->model;
    int mpiReturn;
    clock_t ioTime = 0, cpuTime = 0, lockTime = 0;
    unsigned long int items = 0;
    while (1) {
        clock_t t0 = clock();
        assertMpi(MPI_Bcast(cl, sizeof(Cluster), MPI_BYTE, MFOG_RANK_MAIN, MPI_COMM_WORLD));
        if (cl->label == MFOG_EOS_MARKER) {
            break;
        }
        assertMpi(MPI_Bcast(valuePtr, dim, MPI_DOUBLE, MFOG_RANK_MAIN, MPI_COMM_WORLD));
        cl->center = valuePtr;
        assert(cl->id == model->size);
        items++;
        clock_t t1 = clock();
        ioTime += t1 - t0;
        pthread_rwlock_wrlock(&args->modelLock);
        clock_t t2 = clock();
        lockTime += t2 - t1;
        addCluster(args->minasParams, args->minasState, cl);
        pthread_rwlock_unlock(&args->modelLock);
        if (model->size == args->minasParams->k) {
            for (size_t i = 0; i < args->nClassifiers; i++) {
                sem_post(&args->modelReadySemaphore);
            }
        }
        // new center array, prep for next cluster
        cl->center = calloc(args->minasParams->dim, sizeof(double));
        valuePtr = cl->center;
        clock_t t3 = clock();
        cpuTime += t3 - t2;
    }
    printTiming("m_receiver");
    return NULL;
}

/**
 * @brief In rank 0, sampler thread takes stdin and distributes Examples and (init) Clusters.
 * 
 * @param arg ThreadArgs
 * @return void* ptr to int consumed lines
 */
void *sampler(void *arg) {
    clock_t start = clock();
    ThreadArgs *args = arg;
    fprintf(stderr, "[sampler %d]\n", args->mpiRank);
    Example example;
    double valuePtr[args->minasParams->dim];
    example.val = valuePtr;
    Cluster cluster;
    cluster.center = calloc(args->minasParams->dim, sizeof(double));
    char *lineptr = NULL;
    unsigned long items = 0;
    //
    int clusterBufferLen, exampleBufferLen, valueBufferLen;
    MPI_Pack_size(sizeof(Cluster), MPI_BYTE, MPI_COMM_WORLD, &clusterBufferLen);
    MPI_Pack_size(sizeof(Example), MPI_BYTE, MPI_COMM_WORLD, &exampleBufferLen);
    MPI_Pack_size(args->minasParams->dim, MPI_DOUBLE, MPI_COMM_WORLD, &valueBufferLen);
    int bufferSize = clusterBufferLen + exampleBufferLen + valueBufferLen + 2 * MPI_BSEND_OVERHEAD;
    int mpiReturn, dest = 0;
    int *buffer = (int *)malloc(bufferSize);
    //
    fprintf(stderr, "Taking test stream from stdin, sampler at %d/%d\n", args->mpiRank, args->mpiSize);
    clock_t ioTime = 0, cpuTime = 0, lockTime = 0;
    if (args->outputMode >= MFOG_OUTPUT_MINIMAL) {
        printf("#pointId,label,lag\n");
        fflush(stdout);
    }
    size_t n = 0;
    unsigned long int *itemsWhere = calloc(args->mpiSize, sizeof(unsigned long int));
    // char label[140];
    while (!feof(stdin)) {
        clock_t t0 = clock();
        char lineType = getMfogLine(stdin, &lineptr, &n, args->minasParams->k, args->minasParams->dim, &cluster, &example);
        clock_t t1 = clock();
        ioTime += t1 - t0;
        if (lineType == 'C') {
            if (args->minasState->model.size <= cluster.id) {
                Cluster *cl = addCluster(args->minasParams, args->minasState, &cluster);
                cl->isIntrest = args->outputMode >= MFOG_OUTPUT_ALL;
                cl->latest_match_id = args->minasState->currId;
                clock_t t2 = clock();
                cpuTime += t2 - t1;
                assertMpi(MPI_Bcast(cl, sizeof(Cluster), MPI_BYTE, MFOG_RANK_MAIN, MPI_COMM_WORLD));
                assertMpi(MPI_Bcast(cl->center, args->minasParams->dim, MPI_DOUBLE, MFOG_RANK_MAIN, MPI_COMM_WORLD));
                if (args->minasState->model.size == args->minasParams->k) {
                    fprintf(stderr, "model complete\n");
                }
                if (args->outputMode >= MFOG_OUTPUT_ALL) {
                    printCluster(args->minasParams->dim, cl);
                }
                clock_t t3 = clock();
                ioTime += t3 - t2;
            }
        }
        if (lineType != 'E') {
            continue;
        }
        example.id = args->minasState->nextId;
        example.timeIn = clock();
        args->minasState->nextId++;
        // Match match;
        // identify(args->minasParams, args->minasState, &example, &match);
        // example.label = match.label;
        // minasHandleSleep(args->minasParams, args->minasState);
        // // clock_t t2 = clock();
        // // cpuTime += t2 - t1;
        // //
        // if (args->outputMode >= MFOG_OUTPUT_MINIMAL) {
        //     printf("%10u,%s\n", example.id, printableLabelReuse(example.label, label));
        //     fflush(stdout);
        // }
        // continue;
        //
        int position = 0;
        assertMpi(MPI_Pack(&example, sizeof(Example), MPI_BYTE, buffer, bufferSize, &position, MPI_COMM_WORLD));
        assertMpi(MPI_Pack(example.val, args->minasParams->dim, MPI_DOUBLE, buffer, bufferSize, &position, MPI_COMM_WORLD));
        clock_t t2 = clock();
        cpuTime += t2 - t1;
        do {
            dest = (dest + 1) % args->mpiSize;
        } while (dest == MFOG_RANK_MAIN);
        assertMpi(MPI_Send(buffer, position, MPI_PACKED, dest, MFOG_TAG_EXAMPLE, MPI_COMM_WORLD));
        // fprintf(stderr, "[sampler %d] id %u to %d\n", args->mpiRank, example.id, dest);
        itemsWhere[dest]++;
        clock_t t3 = clock();
        ioTime += t3 - t2;
    }
    char *whereStr = calloc(args->mpiSize * 20, sizeof(char));
    int whereStrTail = 0;
    for (dest = 0; dest < args->mpiSize; dest++) {
        whereStrTail += sprintf(&whereStr[whereStrTail], ", %3d => %8lu", dest, itemsWhere[dest]);
    }
    fprintf(stderr, "[sampler] itemsWhere: %s\n", whereStr);
    free(whereStr);
    free(itemsWhere);
    example.label = MFOG_EOS_MARKER;
    int position = 0;
    assertMpi(MPI_Pack(&example, sizeof(Example), MPI_BYTE, buffer, bufferSize, &position, MPI_COMM_WORLD));
    assertMpi(MPI_Pack(example.val, args->minasParams->dim, MPI_DOUBLE, buffer, bufferSize, &position, MPI_COMM_WORLD));
    for (dest = 1; dest < args->mpiSize; dest++) {
        for (size_t i = 0; i < args->nClassifiers; i++) {
            fprintf(stderr, "[sampler %d] MFOG_EOS_MARKER to %d\n", args->mpiRank, dest);
            assertMpi(MPI_Send(buffer, position, MPI_PACKED, dest, MFOG_TAG_EXAMPLE, MPI_COMM_WORLD));
        }
    }
    printTiming("sampler");
    return NULL;
}

/**
 * @brief In rank 0, detector thread takes Unknowns, stores them, executes
 * novelty detection step and distributes new Cluster's model.
 * 
 * @param arg ThreadArgs
 * @return void* ptr to int consumed lines
 */
void *detector(void *arg) {
    clock_t start = clock();
    ThreadArgs *args = arg;
    fprintf(stderr, "[detector %d]\n", args->mpiRank);
    unsigned long items = 0;
    int streams = args->nClassifiers * (args->mpiSize - 1);
    int endOfStreams = 0;
    Example example;
    double valuePtr[args->minasParams->dim];
    //
    int mpiReturn, exampleBufferLen, valueBufferLen, position;
    MPI_Status st;
    assertMpi(MPI_Pack_size(sizeof(Example), MPI_BYTE, MPI_COMM_WORLD, &exampleBufferLen));
    assertMpi(MPI_Pack_size(args->minasParams->dim, MPI_DOUBLE, MPI_COMM_WORLD, &valueBufferLen));
    int bufferSize = exampleBufferLen + valueBufferLen + 2 * MPI_BSEND_OVERHEAD;
    int *buffer = calloc(bufferSize, sizeof(int));
    char label[20];
    clock_t ioTime = 0, cpuTime = 0, lockTime = 0;
    unsigned long int *itemsWhere = calloc(args->mpiSize, sizeof(unsigned long int));
    while (endOfStreams < streams) {
        clock_t t0 = clock();
        assertMpi(MPI_Recv(buffer, bufferSize, MPI_PACKED, MPI_ANY_SOURCE, MFOG_TAG_UNKNOWN, MPI_COMM_WORLD, &st));
        clock_t t1 = clock();
        ioTime += t1 - t0;
        position = 0;
        assertMpi(MPI_Unpack(buffer, bufferSize, &position, &example, sizeof(Example), MPI_BYTE, MPI_COMM_WORLD));
        assertMpi(MPI_Unpack(buffer, bufferSize, &position, valuePtr, args->minasParams->dim, MPI_DOUBLE, MPI_COMM_WORLD));
        example.val = valuePtr;
        // consider only max id, there may be omitted due to classifier suppression
        if (example.id > args->minasState->currId) {
            args->minasState->currId = example.id;
        }
        items++;
        if (example.label == MFOG_EOS_MARKER) {
            endOfStreams++;
            fprintf(stderr, "[detector %3d] MFOG_EOS_MARKER %d / %d\n", args->mpiRank, endOfStreams, streams);
            continue;
        }
        itemsWhere[st.MPI_SOURCE]++;
        clock_t t2 = clock();
        cpuTime += t2 - t1;
        //
        clock_t t3 = clock();
        ioTime += t3 - t2;
        if (example.label == MINAS_UNK_LABEL) {
            if (args->outputMode >= MFOG_OUTPUT_ALL) {
                printf("Unknown: %20lu", example.id);
                for (unsigned int d = 0; d < args->minasParams->dim; d++)
                    printf(", %le", example.val[d]);
                printf("\n");
                fflush(stdout);
            }
            clock_t t4 = clock();
            ioTime += t4 - t3;
            unsigned int prevSize = args->minasState->model.size;
            // unsigned int nNewClusters = 
            minasHandleUnknown(args->minasParams, args->minasState, &example);
            clock_t t5 = clock();
            cpuTime += t5 - t4;
            for (unsigned long int k = prevSize; k < args->minasState->model.size; k++) {
                Cluster *newCl = &args->minasState->model.clusters[k];
                newCl->isIntrest = args->outputMode >= MFOG_OUTPUT_MINIMAL;
                if (args->outputMode >= MFOG_OUTPUT_ALL) {
                    printCluster(args->minasParams->dim, newCl);
                }
                assertMpi(MPI_Bcast(newCl, sizeof(Cluster), MPI_BYTE, MFOG_RANK_MAIN, MPI_COMM_WORLD));
                assertMpi(MPI_Bcast(newCl->center, args->minasParams->dim, MPI_DOUBLE, MFOG_RANK_MAIN, MPI_COMM_WORLD));
            }
            clock_t t6 = clock();
            ioTime += t6 - t5;
        }
        // if (args->outputMode >= MFOG_OUTPUT_MINIMAL) {
        clock_t lag = clock() - example.timeIn;
        printf("%20lu,%s,%ld\n", example.id, printableLabelReuse(example.label, label), lag);
    }
    char *whereStr = calloc(args->mpiSize * 20, sizeof(char));
    int whereStrTail = 0;
    for (int dest = 0; dest < args->mpiSize; dest++) {
        whereStrTail += sprintf(&whereStr[whereStrTail], ", %3d => %8lu", dest, itemsWhere[dest]);
    }
    fprintf(stderr, "[detector] itemsWhere: %s\n", whereStr);
    free(whereStr);
    free(itemsWhere);
    Cluster cl;
    cl.label = MFOG_EOS_MARKER;
    assertMpi(MPI_Bcast(&cl, sizeof(Cluster), MPI_BYTE, MFOG_RANK_MAIN, MPI_COMM_WORLD));
    //
    free(buffer);
    printTiming("detector  ");
    return NULL;
}

int main(int argc, char const *argv[]) {
    clock_t start = clock();
    MinasParams minasParams = {
        .k=100, .dim=22, .precision=1.0e-08,
        .radiusF=0.25, .minExamplesPerCluster=20, .noveltyF=1.4,
        .thresholdForgettingPast = 10000,
    };
    minasParams.noveltyDetectionTrigger = minasParams.minExamplesPerCluster * minasParams.k;
    minasParams.unknownsMaxSize = minasParams.noveltyDetectionTrigger * 2;
    MinasState minasState = MINAS_STATE_EMPTY;
    minasState.model.size = 0;
    minasState.model.clusters = calloc(minasParams.k, sizeof(Cluster));
    minasState.unknowns = calloc(minasParams.unknownsMaxSize + 1, sizeof(Example));
    for (unsigned long int i = 0; i < minasParams.unknownsMaxSize + 1; i++) {
        minasState.unknowns[i].val = calloc(minasParams.dim, sizeof(double));
    }
    //
    ThreadArgs args = {
        .outputMode = argc >= 2 ? atoi(argv[1]) : MFOG_OUTPUT_ALL,
        .nClassifiers = argc >= 3 ? atoi(argv[2]) : 1,
        .minasParams = &minasParams,
        .minasState = &minasState,
    };
    // printArgs(minasParams, args.outputMode, args.nClassifiers);
    //
    int provided;
    int mpiReturn = MPI_Init_thread(&argc, (char ***)&argv, MPI_THREAD_MULTIPLE, &provided);
    if (mpiReturn != MPI_SUCCESS || provided != MPI_THREAD_MULTIPLE) {
        MPI_Abort(MPI_COMM_WORLD, mpiReturn);
        fail("Erro init programa MPI #%d.\n", mpiReturn);
    }
    MPI_Comm_rank(MPI_COMM_WORLD, &args.mpiRank);
    MPI_Comm_size(MPI_COMM_WORLD, &args.mpiSize);
    assertMsg(args.mpiSize > 1, "This is a multi-process program, got only %d process.", args.mpiSize);
    //
    assertErrno(sem_init(&args.modelReadySemaphore, 0, 0) >= 0, "Semaphore init fail%c.", '.', /**/);
    assertErrno(pthread_rwlock_init(&args.modelLock, NULL) >= 0, "RW-Lock init fail%c.", '.', /**/);
    //
    int result;
    if (args.mpiRank == MFOG_RANK_MAIN) {
        int namelen;
        char mpiProcessorName[MPI_MAX_PROCESSOR_NAME + 1];
        MPI_Get_processor_name(mpiProcessorName, &namelen);
        printArgs(minasParams, args.outputMode, args.nClassifiers);
        pthread_t detector_t;
        assertErrno(pthread_create(&detector_t, NULL, detector, (void *)&args) == 0, "Thread creation fail%c.", '.', MPI_Finalize());
        //
        sampler(&args);
        assertErrno(pthread_join(detector_t, (void **)&result) == 0, "Thread join fail%c.", '.', MPI_Finalize());
    } else {
        pthread_t classifier_t[args.nClassifiers], m_receiver_t;
        for (unsigned long int i = 0; i < args.nClassifiers; i++) {
            assertErrno(pthread_create(&classifier_t[i], NULL, classifier, (void *)&args) == 0, "Thread creation fail%c.", '.', MPI_Finalize());
        }
        assertErrno(pthread_create(&m_receiver_t, NULL, m_receiver, (void *)&args) == 0, "Thread creation fail%c.", '.', MPI_Finalize());
        //
        for (unsigned long int i = 0; i < args.nClassifiers; i++) {
            assertErrno(pthread_join(classifier_t[i], (void **)&result) == 0, "Thread join fail%c.", '.', MPI_Finalize());
        }
        assertErrno(pthread_join(m_receiver_t, (void **)&result) == 0, "Thread join fail%c.", '.', MPI_Finalize());
    }
    restoreSleep(&minasParams, &minasState);
    char *stats = calloc(minasState.model.size * 30, sizeof(char));
    fprintf(stderr, "[node %3d] Statistics %s\n", args.mpiRank, labelMatchStatistics(&minasState.model, stats));
    unsigned int n_matches[minasState.model.size];
    unsigned int n_misses[minasState.model.size];
    for (unsigned long int i = 0; i < minasState.model.size; i++) {
        n_matches[i] = minasState.model.clusters[i].n_matches;
        n_misses[i] = minasState.model.clusters[i].n_misses;
    }
    int gsize;
    assertMpi(MPI_Comm_size(MPI_COMM_WORLD, &gsize));
    unsigned int *root_matches = calloc(gsize * minasState.model.size, sizeof(unsigned int));
    unsigned int *root_misses = calloc(gsize * minasState.model.size, sizeof(unsigned int));
    assertMpi(MPI_Gather(n_matches, minasState.model.size, MPI_UNSIGNED, root_matches, minasState.model.size, MPI_UNSIGNED, MFOG_RANK_MAIN, MPI_COMM_WORLD));
    assertMpi(MPI_Gather(n_misses, minasState.model.size, MPI_UNSIGNED, root_misses, minasState.model.size, MPI_UNSIGNED, MFOG_RANK_MAIN, MPI_COMM_WORLD));
    MPI_Barrier(MPI_COMM_WORLD);
    if (args.mpiRank == MFOG_RANK_MAIN) {
        for (unsigned long int i = 0; i < minasState.model.size; i++) {
            Cluster *cl = &minasState.model.clusters[i];
            cl->n_matches = 0;
            cl->n_misses = 0;
        }
        for (size_t j = 0; j < gsize; j++) {
            unsigned int *node_matches = &root_matches[j * minasState.model.size];
            unsigned int *node_misses  = &root_misses[j * minasState.model.size];
            for (unsigned long int i = 0; i < minasState.model.size; i++) {
                Cluster *cl = &minasState.model.clusters[i];
                cl->n_matches += node_matches[i];
                cl->n_misses += node_misses[i];
            }
        }
        fprintf(stderr, "[root    ] Statistics %s\n", labelMatchStatistics(&minasState.model, stats));
        fprintf(stderr, "[%s %d/%d c%d] %le seconds. At %s:%d\n", argv[0], args.mpiRank, args.mpiSize, args.nClassifiers, ((double)clock() - start) / 1000000.0, __FILE__, __LINE__);
    }
    free(root_matches);
    free(root_misses);
    free(stats);
    free(minasState.model.clusters);
    sem_destroy(&args.modelReadySemaphore);
    assertErrno(pthread_rwlock_destroy(&args.modelLock) >= 0, "RW-Lock destroy fail%c.", '.', /**/);
    return MPI_Finalize();
}
