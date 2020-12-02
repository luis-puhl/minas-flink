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
    int kParam, dim, minExamplesPerCluster;
    double precision, radiusF, noveltyF;
    Model *model;
    char outputMode, nClassifiers;
    // mpi stuff
    int mpiRank, mpiSize;
    char *mpiProcessorName;
    // threading stuff
    int flightControllerBufferSize;
    pthread_mutex_t modelMutex;
    sem_t modelReadySemaphore;
} ThreadArgs;

#define printTiming(who)                                                                                  \
    double ioTime_d = ((double)ioTime) / 1000000.0;                                                       \
    double cpuTime_d = ((double)cpuTime) / 1000000.0;                                                     \
    double lockTime_d = ((double)lockTime) / 1000000.0;                                                   \
    double totalTime_d = ((double)clock() - start) / 1000000.0;                                           \
    fprintf(stderr, "[" who " %3d] (ioTime %le), (cpuTime %le), (lockTime %le), (total %le). At %s:%d\n", \
            args->mpiRank, ioTime_d, cpuTime_d, lockTime_d, totalTime_d, __FILE__, __LINE__);

void *classifier(void *arg) {
    clock_t start = clock();
    ThreadArgs *args = arg;
    Example *example = calloc(1, sizeof(Example));
    example->val = calloc(args->dim, sizeof(double));
    double *valuePtr = example->val;
    fprintf(stderr, "[classifier %d]\n", args->mpiRank);
    Model *model = args->model;
    size_t *inputLine = calloc(1, sizeof(size_t));
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
        assertMpi(MPI_Unpack(buffer, bufferSize, &position, example, sizeof(Example), MPI_BYTE, MPI_COMM_WORLD));
        example->val = valuePtr;
        if (example->label == MFOG_EOS_MARKER) {
            assertMpi(MPI_Send(buffer, position, MPI_PACKED, MFOG_RANK_MAIN, MFOG_TAG_EXAMPLE, MPI_COMM_WORLD));
            break;
        }
        assertMpi(MPI_Unpack(buffer, bufferSize, &position, valuePtr, args->dim, MPI_DOUBLE, MPI_COMM_WORLD));
        clock_t l1 = clock();
        ioTime += (l1 - l0);
        (*inputLine)++;
        //
        while (model->size < args->kParam) {
            fprintf(stderr, "model incomplete %d, wait(modelReadySemaphore);\n", model->size);
            sem_wait(&args->modelReadySemaphore);
            clock_t l2 = clock();
            lockTime += l2 - l1;
        }
        pthread_mutex_lock(&args->modelMutex);
        clock_t l2 = clock();
        lockTime += l2 - l1;
        Match match;
        identify(args->kParam, args->dim, args->precision, args->radiusF, model, example, &match);
        example->label = match.label;
        clock_t l3 = clock();
        cpuTime += (l3 - l2);
        pthread_mutex_unlock(&args->modelMutex);
        //
        position = 0;
        if (example->label == MINAS_UNK_LABEL || match.cluster->isIntrest) {
            assertMpi(MPI_Pack(example, sizeof(Example), MPI_BYTE, buffer, bufferSize, &position, MPI_COMM_WORLD));
            assertMpi(MPI_Pack(example->val, args->dim, MPI_DOUBLE, buffer, bufferSize, &position, MPI_COMM_WORLD));
            assertMpi(MPI_Send(buffer, position, MPI_PACKED, MFOG_RANK_MAIN, MFOG_TAG_EXAMPLE, MPI_COMM_WORLD));
        }
        clock_t l4 = clock();
        ioTime += (l4 - l3);
    }
    free(buffer);
    free(example->val);
    free(example);
    printTiming("classifier");
    return inputLine;
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
    while (1) {
        clock_t t0 = clock();
        assertMpi(MPI_Bcast(cl, sizeof(Cluster), MPI_BYTE, MFOG_RANK_MAIN, MPI_COMM_WORLD));
        if (cl->label == MFOG_EOS_MARKER) {
            // fprintf(stderr, "[m_receiver %d] MFOG_EOS_MARKER\n", args->mpiRank);
            break;
        }
        assertMpi(MPI_Bcast(valuePtr, args->dim, MPI_DOUBLE, MFOG_RANK_MAIN, MPI_COMM_WORLD));
        cl->center = valuePtr;
        assert(cl->id == model->size);
        clock_t t1 = clock();
        ioTime += t1 - t0;
        pthread_mutex_lock(&args->modelMutex);
        clock_t t2 = clock();
        lockTime += t2 - t1;
        if (model->size > 0 && model->size % args->kParam == 0) {
            // fprintf(stderr, "[m_receiver %d] realloc model %d\n", args->mpiRank, model->size);
            model->clusters = realloc(model->clusters, (model->size + args->kParam) * sizeof(Cluster));
        }
        model->clusters[model->size] = *cl;
        model->size++;
        pthread_mutex_unlock(&args->modelMutex);
        if (model->size == args->kParam) {
            // fprintf(stderr, "model complete\n");
            sem_post(&args->modelReadySemaphore);
        }
        // new center array, prep for next cluster
        cl->center = calloc(args->dim, sizeof(double));
        valuePtr = cl->center;
        clock_t t3 = clock();
        cpuTime += t3 - t2;
    }
    printTiming("m_receiver");
    return model;
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
    Example *example = calloc(1, sizeof(Example));
    example->val = calloc(args->dim, sizeof(double));
    Model *model = args->model;
    char *lineptr = NULL;
    size_t n = 0;
    size_t *inputLine = calloc(1, sizeof(size_t));
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
    while (!feof(stdin)) {
        clock_t t0 = clock();
        getline(&lineptr, &n, stdin);
        clock_t t1 = clock();
        ioTime += t1 - t0;
        (*inputLine)++;
        int readCur = 0, readTot = 0, position = 0;
        if (lineptr[0] == 'C') {
            addClusterLine(args->kParam, args->dim, model, lineptr);
            Cluster *cl = &(model->clusters[model->size -1]);
            cl->isIntrest = args->outputMode >= MFOG_OUTPUT_ALL;
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
        for (size_t d = 0; d < args->dim; d++) {
            assert(sscanf(&lineptr[readTot], "%lf,%n", &example->val[d], &readCur));
            readTot += readCur;
        }
        // ignore class
        example->id = id;
        id++;
        //
        assertMpi(MPI_Pack(example, sizeof(Example), MPI_BYTE, buffer, bufferSize, &position, MPI_COMM_WORLD));
        assertMpi(MPI_Pack(example->val, args->dim, MPI_DOUBLE, buffer, bufferSize, &position, MPI_COMM_WORLD));
        clock_t t2 = clock();
        cpuTime += t2 - t1;
        do {
            dest = (dest + 1) % args->mpiSize;
        } while (dest == MFOG_RANK_MAIN);
        assertMpi(MPI_Send(buffer, position, MPI_PACKED, dest, MFOG_TAG_EXAMPLE, MPI_COMM_WORLD));
        clock_t t3 = clock();
        ioTime += t3 - t2;
    }
    example->label = MFOG_EOS_MARKER;
    int position = 0;
    assertMpi(MPI_Pack(example, sizeof(Example), MPI_BYTE, buffer, bufferSize, &position, MPI_COMM_WORLD));
    assertMpi(MPI_Pack(example->val, args->dim, MPI_DOUBLE, buffer, bufferSize, &position, MPI_COMM_WORLD));
    for (dest = 1; dest < args->mpiSize; dest++) {
        // fprintf(stderr, "[sampler %d] MFOG_EOS_MARKER to %d\n", args->mpiRank, dest);
        assertMpi(MPI_Send(buffer, position, MPI_PACKED, dest, MFOG_TAG_EXAMPLE, MPI_COMM_WORLD));
    }
    printTiming("sampler");
    return inputLine;
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
    size_t *inputLine = calloc(1, sizeof(size_t));
    (*inputLine) = 0;
    int streams = args->nClassifiers * (args->mpiSize - 1);
    int kParam = args->kParam, dim = args->dim, minExamplesPerCluster = args->minExamplesPerCluster;
    double precision = args->precision, radiusF = args->radiusF, noveltyF = args->noveltyF;
    Example example;
    example.val = calloc(args->dim, sizeof(double));
    double *valuePtr = example.val;
    //
    size_t noveltyDetectionTrigger = args->minExamplesPerCluster * args->kParam;
    size_t unknownsMaxSize = noveltyDetectionTrigger * 2;
    Example *unknowns = calloc(unknownsMaxSize + 1, sizeof(Example));
    for (size_t i = 0; i < unknownsMaxSize + 1; i++) {
        unknowns[i].val = calloc(args->dim, sizeof(double));
    }
    size_t unknownsSize = 0, lastNDCheck = 0, id = 0;
    //
    int mpiReturn, exampleBufferLen, valueBufferLen, position;
    assertMpi(MPI_Pack_size(sizeof(Example), MPI_BYTE, MPI_COMM_WORLD, &exampleBufferLen));
    assertMpi(MPI_Pack_size(args->dim, MPI_DOUBLE, MPI_COMM_WORLD, &valueBufferLen));
    int bufferSize = exampleBufferLen + valueBufferLen + 2 * MPI_BSEND_OVERHEAD;
    int *buffer = calloc(bufferSize, sizeof(int));
    char label[20];
    clock_t ioTime = 0, cpuTime = 0, lockTime = 0;
    if (args->outputMode >= MFOG_OUTPUT_MINIMAL) {
        printf("#pointId,label\n");
        fflush(stdout);
    }
    while (streams > 0) {
        clock_t t0 = clock();
        assertMpi(MPI_Recv(buffer, bufferSize, MPI_PACKED, MPI_ANY_SOURCE, MFOG_TAG_EXAMPLE, MPI_COMM_WORLD, MPI_STATUS_IGNORE));
        clock_t t1 = clock();
        ioTime += t1 - t0;
        position = 0;
        valuePtr = example.val;
        assertMpi(MPI_Unpack(buffer, bufferSize, &position, &example, sizeof(Example), MPI_BYTE, MPI_COMM_WORLD));
        assertMpi(MPI_Unpack(buffer, bufferSize, &position, valuePtr, args->dim, MPI_DOUBLE, MPI_COMM_WORLD));
        example.val = valuePtr;
        if (example.id > id) {
            id = example.id;
        }
        if (example.label == MFOG_EOS_MARKER) {
            streams--;
            continue;
        }
        clock_t t2 = clock();
        cpuTime += t2 - t1;
        //
        if (args->outputMode >= MFOG_OUTPUT_MINIMAL) {
            printf("%10u,%s\n", example.id, printableLabelReuse(example.label, label));
            fflush(stdout);
        }
        if (example.label != MINAS_UNK_LABEL)
            continue;
        if (args->outputMode >= MFOG_OUTPUT_ALL) {
            printf("Unknown: %10u", example.id);
            for (unsigned int d = 0; d < args->dim; d++)
                printf(", %le", example.val[d]);
            printf("\n");
            fflush(stdout);
        }
        //
        double *sw = unknowns[unknownsSize].val;
        unknowns[unknownsSize] = example;
        example.val = sw;
        unknownsSize++;
        if (unknownsSize >= unknownsMaxSize) {
            // TODO: garbage collect istead of realloc
            // unknownsMaxSize *= 2;
            // unknowns = realloc(unknowns, unknownsMaxSize * sizeof(Example));
            size_t garbageCollected = 0;
            for (size_t ex = 0; ex < unknownsSize; ex++) {
                // compress
                unknowns[ex - garbageCollected] = unknowns[ex];
                if (unknowns[ex].id < lastNDCheck) {
                    garbageCollected++;
                    continue;
                }
            }
            unknownsSize -= garbageCollected;
            fprintf(stderr, "[detector %d] garbageCollect unknowns to %lu "__FILE__":%d\n", args->mpiSize, garbageCollected, __LINE__);
        }
        assert(unknownsSize < unknownsMaxSize);
        //
        if (unknownsSize >= noveltyDetectionTrigger && id - lastNDCheck > noveltyDetectionTrigger) {
            // clock_t ndStart = clock();
            // marker("noveltyDetection");
            Model *model = args->model;
            unsigned int prevSize = model->size;
            noveltyDetection(PARAMS, model, unknowns, unknownsSize);
            unsigned int nNewClusters = model->size - prevSize;
            // clock_t ndEnd = clock();
            // double ndTime = (ndEnd - ndStart) / 1000000.0;
            //
            for (size_t k = prevSize; k < model->size; k++) {
                Cluster *newCl = &model->clusters[k];
                newCl->isIntrest = args->outputMode >= MFOG_OUTPUT_MINIMAL;
                if (args->outputMode >= MFOG_OUTPUT_ALL)
                    printCluster(dim, newCl);
                assertMpi(MPI_Bcast(newCl, sizeof(Cluster), MPI_BYTE, MFOG_RANK_MAIN, MPI_COMM_WORLD));
                assertMpi(MPI_Bcast(newCl->center, args->dim, MPI_DOUBLE, MFOG_RANK_MAIN, MPI_COMM_WORLD));
            }
            // clock_t bcastEnd = clock();
            // double bcastTime = (bcastEnd - ndEnd) / 1000000.0;
            //
            size_t garbageCollected = 0, consumed = 0, reclassified = 0;
            for (size_t ex = 0; ex < unknownsSize; ex++) {
                // compress
                unknowns[ex - (garbageCollected + consumed + reclassified)] = unknowns[ex];
                Cluster *nearest;
                double distance = nearestClusterVal(dim, &model->clusters[prevSize], nNewClusters, unknowns[ex].val, &nearest);
                assert(nearest != NULL);
                if (distance <= nearest->distanceMax) {
                    consumed++;
                    continue;
                }
                distance = nearestClusterVal(dim, model->clusters, model->size - nNewClusters, unknowns[ex].val, &nearest);
                assert(nearest != NULL);
                if (distance <= nearest->distanceMax) {
                    reclassified++;
                    continue;
                }
                if (unknowns[ex].id < lastNDCheck) {
                    garbageCollected++;
                    continue;
                }
            }
            // clock_t compressionEnd = clock();
            // double compressTime = (compressionEnd - bcastEnd) / 1000000.0;
            unknownsSize -= (garbageCollected + consumed + reclassified);
            fprintf(stderr, "ND consumed %lu, reclassified %lu, garbageCollected %lu\n", consumed, reclassified, garbageCollected);
            lastNDCheck = id;
            // double ndTotTime = (clock() - ndStart) / 1000000.0;
            // fprintf(stderr, "ND time(nd=%le, bcast=%le, compress=%le, tot=%le)\n", ndTime, bcastTime, compressTime, ndTotTime);
        }
        clock_t t3 = clock();
        clock_t t4 = clock();
        ioTime += (t1 - t0);
        cpuTime += (t3 - t2);
        lockTime += (t2 - t1) + (t4 - t3);
        // if ((ioTime + cpuTime + lockTime) > 1000000) break;
    }
    Cluster cl;
    cl.label = MFOG_EOS_MARKER;
    assertMpi(MPI_Bcast(&cl, sizeof(Cluster), MPI_BYTE, MFOG_RANK_MAIN, MPI_COMM_WORLD));
    //
    // free(example.val);
    // free(buffer);
    // for (size_t i = 0; i < unknownsMaxSize; i++) {
    //     free(unknowns[i].val);
    // }
    // free(unknowns);
    printTiming("detector");
    return inputLine;
}

int main(int argc, char const *argv[]) {
    clock_t start = clock();
    // assertMsg(argc == 2, "Expected '%s <output level>'", argv[0]);
    ThreadArgs args = {
        .kParam=100, .dim=22, .precision=1.0e-08,
        .radiusF=0.25, .minExamplesPerCluster=20, .noveltyF=1.4,
        .outputMode = argc >= 2 ? atoi(argv[1]) : MFOG_OUTPUT_ALL,
        .nClassifiers = argc >= 3 ? atoi(argv[2]) : 1,
        .flightControllerBufferSize = 1000,
    };
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
    int namelen;
    args.mpiProcessorName = calloc(MPI_MAX_PROCESSOR_NAME + 1, sizeof(char));
    MPI_Get_processor_name(args.mpiProcessorName, &namelen);
    //
    assertErrno(sem_init(&args.modelReadySemaphore, 0, 0) >= 0, "Semaphore init fail%c.", '.', /**/);
    assertErrno(pthread_mutex_init(&args.modelMutex, NULL) >= 0, "Mutex init fail%c.", '.', /**/);
    //
    args.model = calloc(1, sizeof(Model));
    args.model->size = 0;
    args.model->nextLabel = '\0';
    args.model->clusters = calloc(args.kParam, sizeof(Cluster));
    //
    int result;
    if (args.mpiRank == MFOG_RANK_MAIN) {
        fprintf(stderr,
                "%s; kParam=%d; dim=%d; precision=%le; radiusF=%le; minExamplesPerCluster=%d; noveltyF=%le;\n"
                "\tHello from %s, rank %d/%d, outputMode %d, nClassifiers %d\n",
                argv[0],
                args.kParam, args.dim, args.precision, args.radiusF, args.minExamplesPerCluster, args.noveltyF,
                args.mpiProcessorName, args.mpiRank, args.mpiSize, args.outputMode, args.nClassifiers);

        pthread_t detector_t, sampler_t;
        // assertErrno(pthread_create(&sampler_t, NULL, sampler, (void *)&args) == 0, "Thread creation fail%c.", '.', MPI_Finalize());
        assertErrno(pthread_create(&detector_t, NULL, detector, (void *)&args) == 0, "Thread creation fail%c.", '.', MPI_Finalize());
        //
        sampler(&args);
        // assertErrno(pthread_join(sampler_t, (void **)&result) == 0, "Thread join fail%c.", '.', MPI_Finalize());
        assertErrno(pthread_join(detector_t, (void **)&result) == 0, "Thread join fail%c.", '.', MPI_Finalize());
        //
        char *stats = calloc(args.model->size * 30, sizeof(char));
        fprintf(stderr, "[classifier %3d] Statistics: %s\n", args.mpiRank, labelMatchStatistics(args.model, stats));
        Cluster remoteCl;
        #define MFOG_TAG_CLUSTER_STATS 3000
        for (int src = 1; src < args.mpiSize; src++) {
            for (size_t i = 0; i < args.model->size; i++) {
                assertMpi(MPI_Recv(&remoteCl, sizeof(Cluster), MPI_BYTE, MPI_ANY_SOURCE, MFOG_TAG_CLUSTER_STATS, MPI_COMM_WORLD, MPI_STATUS_IGNORE));
                Cluster *cl = &(args.model->clusters[remoteCl.id]);
                assert(cl != NULL && remoteCl.id == cl->id);
                cl->n_matches += remoteCl.n_matches;
                cl->n_misses += remoteCl.n_misses;
            }
        }
        fprintf(stderr, "[root aggregate] Statistics: %s\n", labelMatchStatistics(args.model, stats));
        free(stats);
    } else {
        pthread_t classifier_t[args.nClassifiers], m_receiver_t;
        for (size_t i = 0; i < args.nClassifiers; i++) {
            assertErrno(pthread_create(&classifier_t[i], NULL, classifier, (void *)&args) == 0, "Thread creation fail%c.", '.', MPI_Finalize());
        }
        assertErrno(pthread_create(&m_receiver_t, NULL, m_receiver, (void *)&args) == 0, "Thread creation fail%c.", '.', MPI_Finalize());
        //
        for (size_t i = 0; i < args.nClassifiers; i++) {
            assertErrno(pthread_join(classifier_t[i], (void **)&result) == 0, "Thread join fail%c.", '.', MPI_Finalize());
        }
        assertErrno(pthread_join(m_receiver_t, (void **)&result) == 0, "Thread join fail%c.", '.', MPI_Finalize());
        //
        int mpiReturn;
        for (size_t i = 0; i < args.model->size; i++) {
            Cluster *cl = &(args.model->clusters[i]);
            assertMpi(MPI_Send(cl, sizeof(Cluster), MPI_BYTE, MFOG_RANK_MAIN, MFOG_TAG_CLUSTER_STATS, MPI_COMM_WORLD));
        }
        char *stats = calloc(args.model->size * 30, sizeof(char));
        fprintf(stderr, "[classifier %3d] Statistics: %s\n", args.mpiRank, labelMatchStatistics(args.model, stats));
        free(stats);
    }
    fprintf(stderr, "[%s %d] %le seconds. At %s:%d\n", argv[0], args.mpiRank, ((double)clock() - start) / 1000000.0, __FILE__, __LINE__);
    free(args.model);
    sem_destroy(&args.modelReadySemaphore);
    return MPI_Finalize();
}
