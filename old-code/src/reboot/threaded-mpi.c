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

#define EMPTY_KEY 0
#define SAMPLER_KEY -1
#define DETECTOR_KEY -2

typedef struct {
    pthread_mutex_t modelMutex;
    sem_t modelReadySemaphore;
} LeafState;

typedef struct {
    Example *examples;
    char *slots;
    size_t size;
    pthread_mutex_t slotsMutex;
    sem_t slotFreeSemaphore;
} RootState;

typedef struct {
    int mpiRank, mpiSize;
    int kParam, dim, minExamplesPerCluster;
    double precision, radiusF, noveltyF;
    Model *model;
    RootState rootState;
    LeafState leafState;
} ThreadArgs;


void *classifier(void *arg) {
    clock_t start = clock();
    ThreadArgs *args = arg;
    Example *example = calloc(1, sizeof(Example));
    example->val = calloc(args->dim, sizeof(double));
    double *valuePtr = example->val;
    fprintf(stderr, "[classifier %d]\n", args->mpiRank);
    Model *model = args->model;
    Match match;
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
            match.label = example->label;
            assertMpi(MPI_Send(&match, sizeof(Match), MPI_BYTE, MFOG_RANK_MAIN, MFOG_TAG_UNKNOWN, MPI_COMM_WORLD));
            break;
        }
        assertMpi(MPI_Unpack(buffer, bufferSize, &position, valuePtr, args->dim, MPI_DOUBLE, MPI_COMM_WORLD));
        clock_t l1 = clock();
        ioTime += (l1 - l0);
        (*inputLine)++;
        //
        while (model->size < args->kParam) {
            // fprintf(stderr, "model incomplete %d, wait(modelReadySemaphore);\n", model->size);
            sem_wait(&args->leafState.modelReadySemaphore);
        }
        pthread_mutex_lock(&args->leafState.modelMutex);
        clock_t l2 = clock();
        lockTime += l2 - l1;
        identify(args->kParam, args->dim, args->precision, args->radiusF, model, example, &match);
        clock_t l3 = clock();
        cpuTime += (l3 - l2);
        pthread_mutex_unlock(&args->leafState.modelMutex);
        //
        example->label = match.label;
        assertMpi(MPI_Send(&match, sizeof(Match), MPI_BYTE, MFOG_RANK_MAIN, MFOG_TAG_UNKNOWN, MPI_COMM_WORLD));
        ioTime += (clock() - l3);
    }
    free(buffer);
    free(example->val);
    free(example);
    double ioTime_d = ((double)ioTime) / 1000000.0;
    double cpuTime_d = ((double)cpuTime) / 1000000.0;
    double lockTime_d = ((double)lockTime) / 1000000.0;
    double totalTime_d = ((double)clock() - start) / 1000000.0;
    fprintf(stderr, "[classifier %3d] (ioTime %le), (cpuTime %le), (lockTime %le), (total %le). At %s:%d\n",
            args->mpiRank, ioTime_d, cpuTime_d, lockTime_d, totalTime_d, __FILE__, __LINE__);
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
        clock_t t1 = clock();
        ioTime += t1 - t0;

        // printCluster(args->dim, cl);
        assert(cl->id == model->size);
        pthread_mutex_lock(&args->leafState.modelMutex);
        clock_t t2 = clock();
        lockTime += t2 - t1;
        if (model->size > 0 && model->size % args->kParam == 0) {
            // fprintf(stderr, "[m_receiver %d] realloc model %d\n", args->mpiRank, model->size);
            model->clusters = realloc(model->clusters, (model->size + args->kParam) * sizeof(Cluster));
        }
        model->clusters[model->size] = *cl;
        model->size++;
        pthread_mutex_unlock(&args->leafState.modelMutex);
        if (model->size == args->kParam) {
            // fprintf(stderr, "model complete\n");
            sem_post(&args->leafState.modelReadySemaphore);
        }
        // new center array, prep for next cluster
        cl->center = calloc(args->dim, sizeof(double));
        valuePtr = cl->center;
        clock_t t3 = clock();
        cpuTime += t3 - t2;
    }
    double ioTime_d = ((double)ioTime) / 1000000.0;
    double cpuTime_d = ((double)cpuTime) / 1000000.0;
    double lockTime_d = ((double)lockTime) / 1000000.0;
    double totalTime_d = ((double)clock() - start) / 1000000.0;
    fprintf(stderr, "[m_receiver %3d] (ioTime %le), (cpuTime %le), (lockTime %le), (total %le). At %s:%d\n",
            args->mpiRank, ioTime_d, cpuTime_d, lockTime_d, totalTime_d, __FILE__, __LINE__);
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
    fprintf(stderr, "[sampler    %d]\n", args->mpiRank);
    unsigned int id = 0;
    Example *example;
    Model *model = args->model;
    char *lineptr = NULL;
    size_t n = 0;
    size_t *inputLine = calloc(1, sizeof(size_t));
    //
    int exampleBufferLen, valueBufferLen;
    MPI_Pack_size(sizeof(Example), MPI_BYTE, MPI_COMM_WORLD, &exampleBufferLen);
    MPI_Pack_size(args->dim, MPI_DOUBLE, MPI_COMM_WORLD, &valueBufferLen);
    int bufferSize = exampleBufferLen + valueBufferLen + 2 * MPI_BSEND_OVERHEAD;
    int mpiReturn, dest = 0;
    int *buffer = calloc(bufferSize, sizeof(int));
    // int *destinations = calloc(args->mpiSize, sizeof(int));
    //
    fprintf(stderr, "Taking test stream from stdin, sampler at %d/%d\n", args->mpiRank, args->mpiSize);
    clock_t ioTime = 0, cpuTime = 0, lockTime = 0;
    while (!feof(stdin)) {
        clock_t t0 = clock();
        getline(&lineptr, &n, stdin);
        (*inputLine)++;
        if (lineptr[0] == 'C') {
            pthread_mutex_lock(&args->leafState.modelMutex);
            addClusterLine(args->kParam, args->dim, model, lineptr);
            pthread_mutex_unlock(&args->leafState.modelMutex);
            if (model->size == args->kParam) {
                fprintf(stderr, "model complete\n");
                sem_post(&args->leafState.modelReadySemaphore);
            }
            Cluster *cl = &(model->clusters[model->size -1]);
            cl->n_matches = 0;
            cl->n_misses = 0;
            assertMpi(MPI_Bcast(cl, sizeof(Cluster), MPI_BYTE, MFOG_RANK_MAIN, MPI_COMM_WORLD));
            assertMpi(MPI_Bcast(cl->center, args->dim, MPI_DOUBLE, MFOG_RANK_MAIN, MPI_COMM_WORLD));
            if (model->size >= args->kParam) {
                fprintf(stderr, "model complete\n");
            }
            continue;
        }
        // find space in buff
        clock_t t2 = clock();
        int foundSlot = -1;
        while (1) {
            do {
                dest = (dest + 1) % args->mpiSize;
            } while (dest == MFOG_RANK_MAIN);
            pthread_mutex_lock(&args->rootState.slotsMutex);
            for (size_t i = 0; i < args->rootState.size; i++) {
                char slot = args->rootState.slots[i];
                if (slot == 0 || (slot > args->mpiSize && slot % args->mpiSize == dest)) {
                    foundSlot = i;
                    break;
                }
            }
            if (foundSlot == -1) {
                pthread_mutex_unlock(&args->rootState.slotsMutex);
                sem_wait(&args->rootState.slotFreeSemaphore);
                continue;
            }
            break;
        }
        args->rootState.slots[foundSlot] = dest;
        example = &args->rootState.examples[foundSlot];
        pthread_mutex_unlock(&args->rootState.slotsMutex);
        clock_t t3 = clock();
        // read example
        int readCur = 0, readTot = 0, position = 0;
        for (size_t d = 0; d < args->dim; d++) {
            assert(sscanf(&lineptr[readTot], "%lf,%n", &example->val[d], &readCur));
            readTot += readCur;
        }
        // ignore class
        example->id = id;
        id++;
        clock_t t4 = clock();
        //
        assertMpi(MPI_Pack(example, sizeof(Example), MPI_BYTE, buffer, bufferSize, &position, MPI_COMM_WORLD));
        assertMpi(MPI_Pack(example->val, args->dim, MPI_DOUBLE, buffer, bufferSize, &position, MPI_COMM_WORLD));
        assertMpi(MPI_Send(buffer, position, MPI_PACKED, dest, MFOG_TAG_EXAMPLE, MPI_COMM_WORLD));
        clock_t t5 = clock();
        // fprintf(stderr, "[sampler] send %d to %d"" "__FILE__":%d\n", example->id, dest, __LINE__);
        ioTime += (t2 - t0) + (t5 - t4);
        cpuTime += (t4 - t3);
        lockTime += (t3 - t2);
        if ((ioTime + cpuTime + lockTime) > 20 * 1000000) break;
    }
    example->label = MFOG_EOS_MARKER;
    int position = 0;
    assertMpi(MPI_Pack(example, sizeof(Example), MPI_BYTE, buffer, bufferSize, &position, MPI_COMM_WORLD));
    assertMpi(MPI_Pack(example->val, args->dim, MPI_DOUBLE, buffer, bufferSize, &position, MPI_COMM_WORLD));
    for (dest = 1; dest < args->mpiSize; dest++) {
        assertMpi(MPI_Send(buffer, position, MPI_PACKED, dest, MFOG_TAG_EXAMPLE, MPI_COMM_WORLD));
    }
    double ioTime_d = ((double)ioTime) / 1000000.0;
    double cpuTime_d = ((double)cpuTime) / 1000000.0;
    double lockTime_d = ((double)lockTime) / 1000000.0;
    double totalTime_d = ((double)clock() - start) / 1000000.0;
    fprintf(stderr, "[sampler    %3d] (ioTime %le), (cpuTime %le), (lockTime %le), (total %le). At %s:%d\n",
            args->mpiRank, ioTime_d, cpuTime_d, lockTime_d, totalTime_d, __FILE__, __LINE__);
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
    fprintf(stderr, "[detector   %d]\n", args->mpiRank);
    size_t *inputLine = calloc(1, sizeof(size_t));
    (*inputLine) = 0;
    int streams = args->mpiSize - 1;
    Example *example;
    Match remoteMatch;
    //
    size_t noveltyDetectionTrigger = args->minExamplesPerCluster * args->kParam;
    Example *unknowns = calloc(noveltyDetectionTrigger * 2, sizeof(Example));
    for (size_t i = 0; i < noveltyDetectionTrigger * 2; i++) {
        unknowns[i].val = calloc(args->dim, sizeof(double));
    }
    size_t unknownsSize = 0, lastNDCheck = 0, id = 0;
    //
    Model *model = args->model;
    int mpiReturn;
    MPI_Status st;
    //
    char label[20];
    clock_t ioTime = 0, cpuTime = 0, lockTime = 0;
    printf("#pointId,label\n");
    fflush(stdout);
    while (streams > 0) {
        clock_t t0 = clock();
        assertMpi(MPI_Recv(&remoteMatch, sizeof(Match), MPI_BYTE, MPI_ANY_SOURCE, MFOG_TAG_UNKNOWN, MPI_COMM_WORLD, &st));
        // fprintf(stderr, "[detector] got %d from %d"" "__FILE__":%d\n", remoteMatch.pointId, st.MPI_SOURCE, __LINE__);
        if (remoteMatch.label == MFOG_EOS_MARKER) {
            streams--;
            continue;
        }
        // find ex in buff
        clock_t t1 = clock();
        int foundSlot = -1, dest = st.MPI_SOURCE;
        while (1) {
            pthread_mutex_lock(&args->rootState.slotsMutex);
            for (size_t i = 0; i < args->rootState.size; i++) {
                char slot = args->rootState.slots[i];
                if (slot == dest && args->rootState.examples[i].id == remoteMatch.pointId) {
                    foundSlot = i;
                    break;
                }
            }
            if (foundSlot == -1) {
                pthread_mutex_unlock(&args->rootState.slotsMutex);
                sem_wait(&args->rootState.slotFreeSemaphore);
                continue;
            }
            break;
        }
        example = &args->rootState.examples[foundSlot];
        pthread_mutex_unlock(&args->rootState.slotsMutex);
        clock_t t2 = clock();
        //
        example->label = remoteMatch.label;
        printf("%10u,%s\n", example->id, printableLabelReuse(example->label, label));
        fflush(stdout);
        id = example->id > id ? example->id : id;
        if (example->label == MINAS_UNK_LABEL) {
            printf("Unknown: %10u", example->id);
            for (unsigned int d = 0; d < args->dim; d++)
                printf(", %le", example->val[d]);
            printf("\n");
            fflush(stdout);
            //
            double *sw = unknowns[unknownsSize].val;
            unknowns[unknownsSize] = *example;
            example->val = sw;
            unknownsSize++;
            // if (unknownsSize % args->kParam == 0)
            //     fprintf(stderr, "unknownsSize %lu / %lu\n", unknownsSize, noveltyDetectionTrigger);
            assert(unknownsSize < noveltyDetectionTrigger * 2);
            //
            if (unknownsSize >= noveltyDetectionTrigger && id - lastNDCheck > noveltyDetectionTrigger) {
                clock_t ndStart = clock();
                // marker("noveltyDetection");
                unsigned int prevSize = model->size;
                noveltyDetection(args->kParam, args->dim, args->precision, args->radiusF, args->minExamplesPerCluster, args->noveltyF,
                    model, unknowns, unknownsSize);
                unsigned int nNewClusters = model->size - prevSize;
                clock_t ndEnd = clock();
                double ndTime = (ndEnd - ndStart) / 1000000.0;
                //
                for (size_t k = prevSize; k < model->size; k++) {
                    Cluster *newCl = &model->clusters[k];
                    newCl->n_matches = 0;
                    newCl->n_misses = 0;
                    printCluster(args->dim, newCl);
                    assertMpi(MPI_Bcast(newCl, sizeof(Cluster), MPI_BYTE, MFOG_RANK_MAIN, MPI_COMM_WORLD));
                    assertMpi(MPI_Bcast(newCl->center, args->dim, MPI_DOUBLE, MFOG_RANK_MAIN, MPI_COMM_WORLD));
                }
                clock_t bcastEnd = clock();
                double bcastTime = (bcastEnd - ndEnd) / 1000000.0;
                //
                size_t discarted = 0, consumed = 0, reclassified = 0;
                for (size_t ex = 0; ex < unknownsSize; ex++) {
                    // compress
                    unknowns[ex - (discarted + consumed + reclassified)] = unknowns[ex];
                    Cluster *nearest;
                    double distance = nearestClusterVal(args->dim, &model->clusters[prevSize], nNewClusters, unknowns[ex].val, &nearest);
                    assert(nearest != NULL);
                    if (distance <= nearest->distanceMax) {
                        consumed++;
                        // free(unknowns[ex].val);
                        continue;
                    }
                    distance = nearestClusterVal(args->dim, model->clusters, model->size - nNewClusters, unknowns[ex].val, &nearest);
                    assert(nearest != NULL);
                    if (distance <= nearest->distanceMax) {
                        reclassified++;
                        // free(unknowns[ex].val);
                        continue;
                    }
                    if (unknowns[ex].id < lastNDCheck) {
                        discarted++;
                        // free(unknowns[ex].val);
                        continue;
                    }
                }
                clock_t compressionEnd = clock();
                double compressTime = (compressionEnd - bcastEnd) / 1000000.0;
                fprintf(stderr, "ND consumed %lu, reclassified %lu, discarted %lu\n", consumed, reclassified, discarted);
                unknownsSize -= (discarted + consumed + reclassified);
                lastNDCheck = id;
                double ndTotTime = (clock() - ndStart) / 1000000.0;
                fprintf(stderr, "ND time(nd=%le, bcast=%le, compress=%le, tot=%le)\n", ndTime, bcastTime, compressTime, ndTotTime);
            }
        }
        clock_t t3 = clock();
        pthread_mutex_lock(&args->rootState.slotsMutex);
        args->rootState.slots[foundSlot] += args->mpiSize;
        pthread_mutex_unlock(&args->rootState.slotsMutex);
        sem_post(&args->rootState.slotFreeSemaphore);
        clock_t t4 = clock();
        ioTime += (t1 - t0);
        cpuTime += (t3 - t2);
        lockTime += (t2 - t1) + (t4 - t3);
        // if ((ioTime + cpuTime + lockTime) > 1000000) break;
    }
    Cluster cl;
    cl.label = MFOG_EOS_MARKER;
    assertMpi(MPI_Bcast(&cl, sizeof(Cluster), MPI_BYTE, MFOG_RANK_MAIN, MPI_COMM_WORLD));
    double ioTime_d = ((double)ioTime) / 1000000.0;
    double cpuTime_d = ((double)cpuTime) / 1000000.0;
    double lockTime_d = ((double)lockTime) / 1000000.0;
    double totalTime_d = ((double)clock() - start) / 1000000.0;
    fprintf(stderr, "[detector   %3d] (ioTime %le), (cpuTime %le), (lockTime %le), (total %le). At %s:%d\n",
            args->mpiRank, ioTime_d, cpuTime_d, lockTime_d, totalTime_d, __FILE__, __LINE__);
    return inputLine;
}

int main(int argc, char const *argv[]) {
    clock_t start = clock();
    ThreadArgs args;
    args.kParam=100;
    args.dim=22;
    args.precision=1.0e-08;
    args.radiusF=0.25;
    args.minExamplesPerCluster=20;
    args.noveltyF=1.4;
    int kParam = args.kParam, dim = args.dim, minExamplesPerCluster = args.minExamplesPerCluster;
    double precision = args.precision, radiusF = args.radiusF, noveltyF = args.noveltyF;
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
    char *mpiProcessorName = calloc(MPI_MAX_PROCESSOR_NAME + 1, sizeof(char));
    MPI_Get_processor_name(mpiProcessorName, &namelen);
    //
    assertErrno(sem_init(&args.leafState.modelReadySemaphore, 0, 0) >= 0, "Semaphore init fail%c.", '.', /**/);
    assertErrno(pthread_mutex_init(&args.leafState.modelMutex, NULL) >= 0, "Mutex init fail%c.", '.', /**/);
    //
    args.model = calloc(1, sizeof(Model));
    args.model->size = 0;
    args.model->nextLabel = '\0';
    args.model->clusters = calloc(kParam, sizeof(Cluster));
    //
    int result;
    if (args.mpiRank == MFOG_RANK_MAIN) {
        fprintf(stderr,
            "%s; kParam=%d; dim=%d; precision=%le; radiusF=%le; minExamplesPerCluster=%d; noveltyF=%le;\n"
            "\tHello from %s, rank %d/%d\n",
            argv[0], PARAMS, mpiProcessorName, args.mpiRank, args.mpiSize);
        //
        args.rootState.size = args.kParam *args.minExamplesPerCluster *args.mpiSize;
        args.rootState.examples = calloc(args.rootState.size, sizeof(Example));
        args.rootState.slots = calloc(args.rootState.size, sizeof(char));
        double *allValues = calloc(args.rootState.size * args.dim, sizeof(double));
        for (size_t i = 0; i < args.rootState.size; i++) {
            args.rootState.examples[i].val = &allValues[i * args.dim];
        }
        assertErrno(sem_init(&args.rootState.slotFreeSemaphore, 0, 0) >= 0, "Semaphore init fail%c.", '.', MPI_Finalize());
        assertErrno(pthread_mutex_init(&args.rootState.slotsMutex, NULL) >= 0, "Mutex init fail%c.", '.', MPI_Finalize());

        pthread_t detector_t, sampler_t;
        assertErrno(pthread_create(&sampler_t, NULL, sampler, (void *)&args) == 0, "Thread creation fail%c.", '.', MPI_Finalize());
        assertErrno(pthread_create(&detector_t, NULL, detector, (void *)&args) == 0, "Thread creation fail%c.", '.', MPI_Finalize());
        //
        assertErrno(pthread_join(sampler_t, (void **)&result) == 0, "Thread join fail%c.", '.', MPI_Finalize());
        assertErrno(pthread_join(detector_t, (void **)&result) == 0, "Thread join fail%c.", '.', MPI_Finalize());

        assertErrno(sem_destroy(&args.rootState.slotFreeSemaphore) >= 0, "Semaphore destroy fail%c.", '.', /**/);
        assertErrno(pthread_mutex_destroy(&args.rootState.slotsMutex) >= 0, "Mutex destroy fail%c.", '.', /**/);
        free(allValues);
        free(args.rootState.examples);
        free(args.rootState.slots);
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
        pthread_t classifier_t, m_receiver_t;
        assertErrno(pthread_create(&classifier_t, NULL, classifier, (void *)&args) == 0, "Thread creation fail%c.", '.', MPI_Finalize());
        assertErrno(pthread_create(&m_receiver_t, NULL, m_receiver, (void *)&args) == 0, "Thread creation fail%c.", '.', MPI_Finalize());
        //
        assertErrno(pthread_join(classifier_t, (void **)&result) == 0, "Thread join fail%c.", '.', MPI_Finalize());
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
    // free(args.model->clusters);
    // free(args.model);
    assertErrno(sem_destroy(&args.leafState.modelReadySemaphore) >= 0, "Semaphore destroy fail%c.", '.', /**/);
    assertErrno(pthread_mutex_destroy(&args.leafState.modelMutex) >= 0, "Mutex destroy fail%c.", '.', /**/);
    return MPI_Finalize();
}
