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

typedef struct {
    unsigned int kParam, dim, minExamplesPerCluster, thresholdForgettingPast;
    double precision, radiusF, noveltyF;
    Model *model;
    char outputMode, nClassifiers;//, EOS;
    // mpi stuff
    int mpiRank, mpiSize;
    // pthread_mutex_t inFlightMutex;
    // unsigned long int inFlight;
    // threading stuff
    // pthread_mutex_t modelMutex;
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
    Example example;
    double valuePtr[args->dim];
    Model *model = args->model;
    unsigned long int items = 0;
    //
    int exampleBufferLen, valueBufferLen;
    int mpiReturn;
    assertMpi(MPI_Pack_size(sizeof(Example), MPI_BYTE, MPI_COMM_WORLD, &exampleBufferLen));
    assertMpi(MPI_Pack_size(args->dim, MPI_DOUBLE, MPI_COMM_WORLD, &valueBufferLen));
    int bufferSize = exampleBufferLen + valueBufferLen + 2 * MPI_BSEND_OVERHEAD;
    int *buffer = (int *)malloc(bufferSize);
    clock_t ioTime = 0, cpuTime = 0, lockTime = 0;
    while (1) {
        clock_t l0 = clock();
        assertMpi(MPI_Recv(buffer, bufferSize, MPI_PACKED, MPI_ANY_SOURCE, MFOG_TAG_EXAMPLE, MPI_COMM_WORLD, MPI_STATUS_IGNORE));
        int position = 0;
        assertMpi(MPI_Unpack(buffer, bufferSize, &position, &example, sizeof(Example), MPI_BYTE, MPI_COMM_WORLD));
        assertMpi(MPI_Unpack(buffer, bufferSize, &position, valuePtr, args->dim, MPI_DOUBLE, MPI_COMM_WORLD));
        example.val = valuePtr;
        if (example.label == MFOG_EOS_MARKER) {
            assertMpi(MPI_Send(buffer, position, MPI_PACKED, MFOG_RANK_MAIN, MFOG_TAG_UNKNOWN, MPI_COMM_WORLD));
            break;
        }
        clock_t l1 = clock();
        ioTime += (l1 - l0);
        //
        while (model->size < args->kParam) {
            sem_wait(&args->modelReadySemaphore);
        }
        // pthread_mutex_lock(&args->modelMutex);
        pthread_rwlock_rdlock(&args->modelLock);
        clock_t l2 = clock();
        lockTime += l2 - l1;
        Match match;
        identify(args->kParam, args->dim, args->precision, args->radiusF, model, &example, &match, args->thresholdForgettingPast);
        items++;
        example.label = match.label;
        clock_t l3 = clock();
        cpuTime += (l3 - l2);
        // pthread_mutex_unlock(&args->modelMutex);
        pthread_rwlock_unlock(&args->modelLock);
        //
        position = 0;
        if (example.label == MINAS_UNK_LABEL || match.cluster->isIntrest) {
            assertMpi(MPI_Pack(&example, sizeof(Example), MPI_BYTE, buffer, bufferSize, &position, MPI_COMM_WORLD));
            assertMpi(MPI_Pack(valuePtr, args->dim, MPI_DOUBLE, buffer, bufferSize, &position, MPI_COMM_WORLD));
            assertMpi(MPI_Send(buffer, position, MPI_PACKED, MFOG_RANK_MAIN, MFOG_TAG_UNKNOWN, MPI_COMM_WORLD));
        }
        clock_t l4 = clock();
        ioTime += (l4 - l3);
    }
    free(buffer);
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
    fprintf(stderr, "[m_receiver %d]\n", args->mpiRank);
    Cluster *cl = calloc(1, sizeof(Cluster));
    cl->center = calloc(args->dim, sizeof(double));
    double *valuePtr = cl->center;
    int mpiReturn;
    Model *model = args->model;
    clock_t ioTime = 0, cpuTime = 0, lockTime = 0;
    unsigned long int items = 0;
    while (1) {
        clock_t t0 = clock();
        assertMpi(MPI_Bcast(cl, sizeof(Cluster), MPI_BYTE, MFOG_RANK_MAIN, MPI_COMM_WORLD));
        if (cl->label == MFOG_EOS_MARKER) {
            // args->EOS = 1;
            // fprintf(stderr, "[m_receiver %d] MFOG_EOS_MARKER\n", args->mpiRank);
            break;
        }
        assertMpi(MPI_Bcast(valuePtr, args->dim, MPI_DOUBLE, MFOG_RANK_MAIN, MPI_COMM_WORLD));
        cl->center = valuePtr;
        assert(cl->id == model->size);
        items++;
        clock_t t1 = clock();
        ioTime += t1 - t0;
        // pthread_mutex_lock(&args->modelMutex);
        pthread_rwlock_wrlock(&args->modelLock);
        clock_t t2 = clock();
        lockTime += t2 - t1;
        if (model->size > 0 && model->size % args->kParam == 0) {
            // fprintf(stderr, "[m_receiver %d] realloc model %d\n", args->mpiRank, model->size);
            model->clusters = realloc(model->clusters, (model->size + args->kParam) * sizeof(Cluster));
        }
        cl->n_matches = 0;
        cl->n_misses = 0;
        model->clusters[model->size] = *cl;
        model->size++;
        // pthread_mutex_unlock(&args->modelMutex);
        pthread_rwlock_unlock(&args->modelLock);
        if (model->size == args->kParam) {
            // fprintf(stderr, "model complete\n");
            for (size_t i = 0; i < args->nClassifiers; i++) {
                sem_post(&args->modelReadySemaphore);
            }
        }
        // new center array, prep for next cluster
        cl->center = calloc(args->dim, sizeof(double));
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
    unsigned int id = 0;
    Example example;
    double valuePtr[args->dim];
    example.val = valuePtr;
    Cluster cluster;
    cluster.center = calloc(args->dim, sizeof(double));
    Model *model = args->model;
    char *lineptr = NULL;
    unsigned long int items = 0;
    //
    int clusterBufferLen, exampleBufferLen, valueBufferLen;
    MPI_Pack_size(sizeof(Cluster), MPI_BYTE, MPI_COMM_WORLD, &clusterBufferLen);
    MPI_Pack_size(sizeof(Example), MPI_BYTE, MPI_COMM_WORLD, &exampleBufferLen);
    MPI_Pack_size(args->dim, MPI_DOUBLE, MPI_COMM_WORLD, &valueBufferLen);
    int bufferSize = clusterBufferLen + exampleBufferLen + valueBufferLen + 2 * MPI_BSEND_OVERHEAD;
    int mpiReturn, dest = 0;
    int *buffer = (int *)malloc(bufferSize);
    //
    fprintf(stderr, "Taking test stream from stdin, sampler at %d/%d\n", args->mpiRank, args->mpiSize);
    clock_t ioTime = 0, cpuTime = 0, lockTime = 0;
    if (args->outputMode >= MFOG_OUTPUT_MINIMAL) {
        printf("#pointId,label\n");
        fflush(stdout);
    }
    size_t n = 0;
    unsigned long int *itemsWhere = calloc(args->mpiSize, sizeof(unsigned long int));
    while (!feof(stdin)) {
        clock_t t0 = clock();
        getline(&lineptr, &n, stdin);
        clock_t t1 = clock();
        ioTime += t1 - t0;
        items++;
        int readCur = 0, readTot = 0, position = 0;
        if (lineptr[0] == 'C') {
            readCluster(args->kParam, args->dim, &cluster, lineptr);
            addCluster(args->kParam, args->dim, &cluster, model);
            Cluster *cl = &(model->clusters[model->size -1]);
            cl->isIntrest = args->outputMode >= MFOG_OUTPUT_ALL;
            cl->n_matches = 0;
            cl->n_misses = 0;
            cl->latest_match_id = id;
            if (args->outputMode >= MFOG_OUTPUT_ALL)
                printCluster(args->dim, cl);
            clock_t t2 = clock();
            cpuTime += t2 - t1;
            assertMpi(MPI_Bcast(cl, sizeof(Cluster), MPI_BYTE, MFOG_RANK_MAIN, MPI_COMM_WORLD));
            assertMpi(MPI_Bcast(cl->center, args->dim, MPI_DOUBLE, MFOG_RANK_MAIN, MPI_COMM_WORLD));
            if (model->size >= args->kParam) {
                fprintf(stderr, "model complete\n");
            }
            clock_t t3 = clock();
            ioTime += t3 - t2;
            continue;
        }
        for (unsigned long int d = 0; d < args->dim; d++) {
            assert(sscanf(&lineptr[readTot], "%lf,%n", &example.val[d], &readCur));
            readTot += readCur;
        }
        // ignore class
        example.id = id;
        id++;
        //
        assertMpi(MPI_Pack(&example, sizeof(Example), MPI_BYTE, buffer, bufferSize, &position, MPI_COMM_WORLD));
        assertMpi(MPI_Pack(example.val, args->dim, MPI_DOUBLE, buffer, bufferSize, &position, MPI_COMM_WORLD));
        clock_t t2 = clock();
        cpuTime += t2 - t1;
        do {
            dest = (dest + 1) % args->mpiSize;
        } while (dest == MFOG_RANK_MAIN);
        // pthread_mutex_lock(&args->inFlightMutex);
        // args->inFlight++;
        // pthread_mutex_unlock(&args->inFlightMutex);
        assertMpi(MPI_Send(buffer, position, MPI_PACKED, dest, MFOG_TAG_EXAMPLE, MPI_COMM_WORLD));
        itemsWhere[dest]++;
        clock_t t3 = clock();
        ioTime += t3 - t2;
    }
    char *whereStr = calloc(args->mpiSize * 20, sizeof(char));
    int whereStrTail = 0;
    for (dest = 0; dest < args->mpiSize; dest++) {
        whereStrTail += sprintf(&whereStr[whereStrTail], ", %3d => %8lu", dest, itemsWhere[dest]);
    }
    fprintf(stderr, "[sampler] args->inFlight %lu, itemsWhere: %s\n", 0l /* args->inFlight */, whereStr);
    free(whereStr);
    free(itemsWhere);
    // args->EOS = 1;
    example.label = MFOG_EOS_MARKER;
    int position = 0;
    assertMpi(MPI_Pack(&example, sizeof(Example), MPI_BYTE, buffer, bufferSize, &position, MPI_COMM_WORLD));
    assertMpi(MPI_Pack(example.val, args->dim, MPI_DOUBLE, buffer, bufferSize, &position, MPI_COMM_WORLD));
    for (dest = 1; dest < args->mpiSize; dest++) {
        for (size_t i = 0; i < args->nClassifiers; i++) {
            fprintf(stderr, "[sampler %d] MFOG_EOS_MARKER to %d\n", args->mpiRank, dest);
            assertMpi(MPI_Send(buffer, position, MPI_PACKED, dest, MFOG_TAG_EXAMPLE, MPI_COMM_WORLD));
        }
    }
    // assertMpi(MPI_Send(buffer, position, MPI_PACKED, 0, MFOG_TAG_EXAMPLE, MPI_COMM_WORLD));
    // Cluster cl;
    // cl.label = MFOG_EOS_MARKER;
    // assertMpi(MPI_Bcast(&cl, sizeof(Cluster), MPI_BYTE, MFOG_RANK_MAIN, MPI_COMM_WORLD));
    //
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
    unsigned long int items = 0;
    int streams = args->nClassifiers * (args->mpiSize - 1);
    int endOfStreams = 0;
    int kParam = args->kParam, dim = args->dim, minExamplesPerCluster = args->minExamplesPerCluster;
    double precision = args->precision, radiusF = args->radiusF, noveltyF = args->noveltyF;
    Example example;
    double valuePtr[args->dim];
    //
    unsigned long int noveltyDetectionTrigger = args->minExamplesPerCluster * args->kParam;
    unsigned long int unknownsMaxSize = noveltyDetectionTrigger * 2;
    Example *unknowns = calloc(unknownsMaxSize + 1, sizeof(Example));
    for (unsigned long int i = 0; i < unknownsMaxSize + 1; i++) {
        unknowns[i].val = calloc(args->dim, sizeof(double));
    }
    unsigned long int unknownsSize = 0, lastNDCheck = 0, id = 0;
    //
    int mpiReturn, exampleBufferLen, valueBufferLen, position;
    MPI_Status st;
    assertMpi(MPI_Pack_size(sizeof(Example), MPI_BYTE, MPI_COMM_WORLD, &exampleBufferLen));
    assertMpi(MPI_Pack_size(args->dim, MPI_DOUBLE, MPI_COMM_WORLD, &valueBufferLen));
    int bufferSize = exampleBufferLen + valueBufferLen + 2 * MPI_BSEND_OVERHEAD;
    int *buffer = calloc(bufferSize, sizeof(int));
    char label[20];
    clock_t ioTime = 0, cpuTime = 0, lockTime = 0;
    // unsigned long int maxInFlight = 0;
    unsigned long int *itemsWhere = calloc(args->mpiSize, sizeof(unsigned long int));
    while (endOfStreams < streams) {
        // if (maxInFlight < args->inFlight) {
        //     maxInFlight = args->inFlight;
        // }
        // if (args->EOS && args->inFlight % (maxInFlight / 10) == 0) {
        //     fprintf(stderr, "[detector] args->inFlight %8lu (%3lu %%)\n", args->inFlight, 10 * (args->inFlight / (maxInFlight / 10)));
        // }
        clock_t t0 = clock();
        assertMpi(MPI_Recv(buffer, bufferSize, MPI_PACKED, MPI_ANY_SOURCE, MFOG_TAG_UNKNOWN, MPI_COMM_WORLD, &st));
        clock_t t1 = clock();
        ioTime += t1 - t0;
        position = 0;
        assertMpi(MPI_Unpack(buffer, bufferSize, &position, &example, sizeof(Example), MPI_BYTE, MPI_COMM_WORLD));
        assertMpi(MPI_Unpack(buffer, bufferSize, &position, valuePtr, args->dim, MPI_DOUBLE, MPI_COMM_WORLD));
        example.val = valuePtr;
        // pthread_mutex_lock(&args->inFlightMutex);
        // args->inFlight--;
        // pthread_mutex_unlock(&args->inFlightMutex);
        // consider only max id, there may be omitted due to classifier suppression
        if (example.id > id) {
            id = example.id;
        }
        items++;
        if (example.label == MFOG_EOS_MARKER) {
            endOfStreams++;
            fprintf(stderr, "[detector %3d] MFOG_EOS_MARKER %d / %d \n", args->mpiRank, endOfStreams, streams);
            continue;
        }
        itemsWhere[st.MPI_SOURCE]++;
        clock_t t2 = clock();
        cpuTime += t2 - t1;
        //
        // if (args->outputMode >= MFOG_OUTPUT_MINIMAL) {
        printf("%10u,%s\n", example.id, printableLabelReuse(example.label, label));
        fflush(stdout);
        clock_t t3 = clock();
        ioTime += t3 - t2;
        if (example.label != MINAS_UNK_LABEL)
            continue;
        if (args->outputMode >= MFOG_OUTPUT_ALL) {
            printf("Unknown: %10u", example.id);
            for (unsigned int d = 0; d < args->dim; d++)
                printf(", %le", example.val[d]);
            printf("\n");
            fflush(stdout);
        }
        clock_t t4 = clock();
        ioTime += t4 - t3;
        //
        unknowns[unknownsSize].id = example.id;
        unknowns[unknownsSize].label = example.label;
        for (size_t i = 0; i < args->dim; i++) {
            unknowns[unknownsSize].val[i] = valuePtr[i];
        }
        unknownsSize++;
        if (unknownsSize >= unknownsMaxSize) {
            unsigned long int garbageCollected = 0;
            for (unsigned long int ex = 0; ex < unknownsSize; ex++) {
                // compress
                unknowns[ex - garbageCollected] = unknowns[ex];
                if (unknowns[ex].id < lastNDCheck) {
                    garbageCollected++;
                    continue;
                }
            }
            unknownsSize -= garbageCollected;
            clock_t t5 = clock();
            cpuTime += t5 - t4;
            fprintf(stderr, "[detector %d] garbageCollect unknowns to %lu "__FILE__":%d\n", args->mpiSize, garbageCollected, __LINE__);
        }
        assert(unknownsSize < unknownsMaxSize);
        //
        if (unknownsSize >= noveltyDetectionTrigger && id - lastNDCheck > noveltyDetectionTrigger) {
            Model *model = args->model;
            unsigned int prevSize = model->size, noveltyCount;
            unsigned int nNewClusters = noveltyDetection(kParam, dim, precision, radiusF, args->thresholdForgettingPast, minExamplesPerCluster, noveltyF, model, unknowns, unknownsSize, &noveltyCount);
            clock_t t3 = clock();
            cpuTime += t3 - t2;
            //
            for (unsigned long int k = prevSize; k < model->size; k++) {
                Cluster *newCl = &model->clusters[k];
                newCl->isIntrest = args->outputMode >= MFOG_OUTPUT_MINIMAL;
                newCl->n_matches = 0;
                newCl->n_misses = 0;
                newCl->latest_match_id = id;
                if (args->outputMode >= MFOG_OUTPUT_ALL)
                    printCluster(dim, newCl);
                assertMpi(MPI_Bcast(newCl, sizeof(Cluster), MPI_BYTE, MFOG_RANK_MAIN, MPI_COMM_WORLD));
                assertMpi(MPI_Bcast(newCl->center, args->dim, MPI_DOUBLE, MFOG_RANK_MAIN, MPI_COMM_WORLD));
            }
            clock_t t4 = clock();
            ioTime += t4 - t3;
            //
            unsigned long int garbageCollected = 0, consumed = 0, reclassified = 0;
            for (unsigned long int ex = 0; ex < unknownsSize; ex++) {
                // compress
                unknowns[ex - (garbageCollected + consumed + reclassified)] = unknowns[ex];
                Cluster *nearest;
                double distance = nearestClusterVal(dim, &model->clusters[prevSize], nNewClusters, unknowns[ex].val, &nearest);
                assert(nearest != NULL);
                if (distance <= nearest->distanceMax) {
                    consumed++;
                    continue;
                }
                // distance = nearestClusterVal(dim, model->clusters, model->size - nNewClusters, unknowns[ex].val, &nearest);
                // assert(nearest != NULL);
                // if (distance <= nearest->distanceMax) {
                //     reclassified++;
                //     continue;
                // }
                if (unknowns[ex].id < lastNDCheck) {
                    garbageCollected++;
                    continue;
                }
            }
            clock_t t5 = clock();
            cpuTime += t5 - t4;
            unknownsSize -= (garbageCollected + consumed + reclassified);
            fprintf(stderr, "Novelties %3u, Extensions %3u, consumed %6lu, reclassified %6lu, garbageCollected %6lu\n",
                    noveltyCount, nNewClusters - noveltyCount, consumed, reclassified, garbageCollected);
            lastNDCheck = id;
        }
    }
    char *whereStr = calloc(args->mpiSize * 20, sizeof(char));
    int whereStrTail = 0;
    for (int dest = 0; dest < args->mpiSize; dest++) {
        whereStrTail += sprintf(&whereStr[whereStrTail], ", %3d => %8lu", dest, itemsWhere[dest]);
    }
    fprintf(stderr, "[detector] args->inFlight %lu, itemsWhere: %s\n", 0l /* args->inFlight */, whereStr);
    free(whereStr);
    free(itemsWhere);
    Cluster cl;
    cl.label = MFOG_EOS_MARKER;
    assertMpi(MPI_Bcast(&cl, sizeof(Cluster), MPI_BYTE, MFOG_RANK_MAIN, MPI_COMM_WORLD));
    //
    free(buffer);
    // for (unsigned long int i = 0; i < unknownsMaxSize; i++) {
    //     free(unknowns[i].val);
    // }
    free(unknowns);
    printTiming("detector  ");
    return NULL;
}

int main(int argc, char const *argv[]) {
    clock_t start = clock();
    ThreadArgs args = {
        .kParam=100, .dim=22, .precision=1.0e-08,
        .radiusF=0.25, .minExamplesPerCluster=20, .noveltyF=1.4,
        .outputMode = argc >= 2 ? atoi(argv[1]) : MFOG_OUTPUT_ALL,
        .nClassifiers = argc >= 3 ? atoi(argv[2]) : 1,
        // .EOS = 0,
        // .inFlight = 0,
        .thresholdForgettingPast = 10000,
    };
    // unsigned int kParam = 100, dim = 22, minExamplesPerCluster = 20, thresholdForgettingPast = 10000;
    // double precision = 1.0e-08, radiusF = 0.25, noveltyF = 1.4;
    // fprintf(stderr, "%s; kParam=%u; dim=%u; precision=%le; radiusF=%le; minExamplesPerCluster=%u; noveltyF=%le, thresholdForgettingPast=%u\n", argv[0], PARAMS);
    //
    // if (argc == 2) stdin = fopen(argv[1], "r");
    int provided;
    int mpiReturn = MPI_Init_thread(&argc, (char ***)&argv, MPI_THREAD_MULTIPLE, &provided);
    if (mpiReturn != MPI_SUCCESS || provided != MPI_THREAD_MULTIPLE) {
        MPI_Abort(MPI_COMM_WORLD, mpiReturn);
        fail("Erro iniciando programa MPI #%d.\n", mpiReturn);
    }
    MPI_Comm_rank(MPI_COMM_WORLD, &args.mpiRank);
    MPI_Comm_size(MPI_COMM_WORLD, &args.mpiSize);
    assertMsg(args.mpiSize > 1, "This is a multi-process program, got only %d process.", args.mpiSize);
    //
    assertErrno(sem_init(&args.modelReadySemaphore, 0, 0) >= 0, "Semaphore init fail%c.", '.', /**/);
    // assertErrno(pthread_mutex_init(&args.modelMutex, NULL) >= 0, "Mutex init fail%c.", '.', /**/);
    assertErrno(pthread_rwlock_init(&args.modelLock, NULL) >= 0, "RW-Lock init fail%c.", '.', /**/);
    // assertErrno(pthread_mutex_init(&args.inFlightMutex, NULL) >= 0, "Mutex init fail%c.", '.', /**/);
    //
    args.model = calloc(1, sizeof(Model));
    args.model->size = 0;
    args.model->nextLabel = '\0';
    args.model->clusters = calloc(args.kParam, sizeof(Cluster));
    //
    int result;
    if (args.mpiRank == MFOG_RANK_MAIN) {
        int namelen;
        char mpiProcessorName[MPI_MAX_PROCESSOR_NAME + 1];
        MPI_Get_processor_name(mpiProcessorName, &namelen);
        fprintf(stderr,
                "%s; kParam=%d; dim=%d; precision=%le; radiusF=%le; minExamplesPerCluster=%d; noveltyF=%le;\n"
                "\tHello from %s, rank %d/%d, outputMode %d, nClassifiers %d\n",
                argv[0],
                args.kParam, args.dim, args.precision, args.radiusF, args.minExamplesPerCluster, args.noveltyF,
                mpiProcessorName, args.mpiRank, args.mpiSize, args.outputMode, args.nClassifiers);

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
    char *stats = calloc(args.model->size * 30, sizeof(char));
    fprintf(stderr, "[node %3d] Statistics %s\n", args.mpiRank, labelMatchStatistics(args.model, stats));
    unsigned int n_matches[args.model->size];
    unsigned int n_misses[args.model->size];
    for (unsigned long int i = 0; i < args.model->size; i++) {
        n_matches[i] = args.model->clusters[i].n_matches;
        n_misses[i] = args.model->clusters[i].n_misses;
    }
    int gsize;
    assertMpi(MPI_Comm_size(MPI_COMM_WORLD, &gsize));
    unsigned int *root_matches = calloc(gsize * args.model->size, sizeof(unsigned int));
    unsigned int *root_misses = calloc(gsize * args.model->size, sizeof(unsigned int));
    assertMpi(MPI_Gather(n_matches, args.model->size, MPI_UNSIGNED, root_matches, args.model->size, MPI_UNSIGNED, MFOG_RANK_MAIN, MPI_COMM_WORLD));
    assertMpi(MPI_Gather(n_misses, args.model->size, MPI_UNSIGNED, root_misses, args.model->size, MPI_UNSIGNED, MFOG_RANK_MAIN, MPI_COMM_WORLD));
    MPI_Barrier(MPI_COMM_WORLD);
    if (args.mpiRank == MFOG_RANK_MAIN) {
        for (unsigned long int i = 0; i < args.model->size; i++) {
            Cluster *cl = &args.model->clusters[i];
            cl->n_matches = 0;
            cl->n_misses = 0;
        }
        for (size_t j = 0; j < gsize; j++) {
            unsigned int *node_matches = &root_matches[j * args.model->size];
            unsigned int *node_misses  = &root_misses[j * args.model->size];
            for (unsigned long int i = 0; i < args.model->size; i++) {
                Cluster *cl = &args.model->clusters[i];
                cl->n_matches += node_matches[i];
                cl->n_misses += node_misses[i];
            }
        }
        fprintf(stderr, "[root    ] Statistics %s\n", labelMatchStatistics(args.model, stats));
        fprintf(stderr, "[%s %d/%d] %le seconds. At %s:%d\n", argv[0], args.mpiRank, args.mpiSize, ((double)clock() - start) / 1000000.0, __FILE__, __LINE__);
    }
    free(root_matches);
    free(root_misses);
    free(stats);
    free(args.model);
    sem_destroy(&args.modelReadySemaphore);
    // pthread_mutex_destroy(&args.modelMutex)
    assertErrno(pthread_rwlock_destroy(&args.modelLock) >= 0, "RW-Lock destroy fail%c.", '.', /**/);
    // pthread_mutex_destroy(&args.inFlightMutex);
    return MPI_Finalize();
}
