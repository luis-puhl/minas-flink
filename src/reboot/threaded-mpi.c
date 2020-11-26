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
#include "./CircularExampleBuffer.h"

#define assertMpi(exp)                        \
    if ((mpiReturn = exp) != MPI_SUCCESS) { \
        MPI_Abort(MPI_COMM_WORLD, mpiReturn); \
        errx(EXIT_FAILURE, "MPI Abort %d. At "__FILE__":%d\n", mpiReturn, __LINE__); }
#define MFOG_RANK_MAIN 0
#define MFOG_TAG_EXAMPLE 2004
#define MFOG_TAG_UNKNOWN 2005
#define MFOG_EOS_MARKER '\127'

typedef struct {
    int kParam, dim, minExamplesPerCluster;
    double precision, radiusF, noveltyF;
    // mpi stuff
    int mpiRank, mpiSize;
    char *mpiProcessorName;
    CircularExampleBuffer *flightController;
    Model *model;
    pthread_mutex_t modelMutex;
    sem_t modelReadySemaphore;
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
    // MPI_Buffer_attach(buffer, bufferSize);
    while (1) {
        assertMpi(MPI_Recv(buffer, bufferSize, MPI_PACKED, MPI_ANY_SOURCE, MFOG_TAG_EXAMPLE, MPI_COMM_WORLD, MPI_STATUS_IGNORE));
        int position = 0;
        assertMpi(MPI_Unpack(buffer, bufferSize, &position, example, sizeof(Example), MPI_BYTE, MPI_COMM_WORLD));
        example->val = valuePtr;
        if (example->label == MFOG_EOS_MARKER) {
            assertMpi(MPI_Send(example, sizeof(Example), MPI_BYTE, MFOG_RANK_MAIN, MFOG_TAG_UNKNOWN, MPI_COMM_WORLD));
            break;
        }
        assertMpi(MPI_Unpack(buffer, bufferSize, &position, valuePtr, args->dim, MPI_DOUBLE, MPI_COMM_WORLD));
        (*inputLine)++;
        //
        while (model->size < args->kParam) {
            fprintf(stderr, "model incomplete %d, wait(modelReadySemaphore);\n", model->size);
            sem_wait(&args->modelReadySemaphore);
        }
        pthread_mutex_lock(&args->modelMutex);
        identify(args->kParam, args->dim, args->precision, args->radiusF, model, example, &match);
        pthread_mutex_unlock(&args->modelMutex);
        //
        example->label = match.label;
        assertMpi(MPI_Send(example, sizeof(Example), MPI_BYTE, MFOG_RANK_MAIN, MFOG_TAG_UNKNOWN, MPI_COMM_WORLD));
    }
    fprintf(stderr, "[classifier %d] %le seconds. At %s:%d\n", args->mpiRank, ((double)clock() - start) / 1000000.0, __FILE__, __LINE__);
    free(buffer);
    free(example->val);
    free(example);
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
    // fprintf(stderr, "m_receiver at %d/%d\n", args->mpiRank, args->mpiSize);
    while (1) {
        assertMpi(MPI_Bcast(cl, sizeof(Cluster), MPI_BYTE, MFOG_RANK_MAIN, MPI_COMM_WORLD));
        if (cl->label == MFOG_EOS_MARKER) {
            // fprintf(stderr, "[m_receiver %d] MFOG_EOS_MARKER\n", args->mpiRank);
            break;
        }
        assertMpi(MPI_Bcast(valuePtr, args->dim, MPI_DOUBLE, MFOG_RANK_MAIN, MPI_COMM_WORLD));
        cl->center = valuePtr;

        // printCluster(args->dim, cl);
        assert(cl->id == model->size);
        pthread_mutex_lock(&args->modelMutex);
        if (model->size > 0 && model->size % args->kParam == 0) {
            fprintf(stderr, "[m_receiver %d] realloc model %d\n", args->mpiRank, model->size);
            model->clusters = realloc(model->clusters, (model->size + args->kParam) * sizeof(Cluster));
        }
        model->clusters[model->size] = *cl;
        model->size++;
        pthread_mutex_unlock(&args->modelMutex);
        if (model->size == args->kParam) {
            fprintf(stderr, "model complete\n");
            sem_post(&args->modelReadySemaphore);
        }
        // new center array, prep for next cluster
        cl->center = calloc(args->dim, sizeof(double));
        valuePtr = cl->center;
    }
    fprintf(stderr, "[m_receiver %d] %le seconds. At %s:%d\n", args->mpiRank, ((double)clock() - start) / 1000000.0, __FILE__, __LINE__);
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
    fprintf(stderr, "Taking test stream from stdin\n");
    fprintf(stderr, "sampler at %d/%d\n", args->mpiRank, args->mpiSize);
    while (!feof(stdin)) {
        getline(&lineptr, &n, stdin);
        (*inputLine)++;
        int readCur = 0, readTot = 0, position = 0;
        if (lineptr[0] == 'C') {
            addClusterLine(args->kParam, args->dim, model, lineptr);
            Cluster *cl = &(model->clusters[model->size -1]);
            // printCluster(args->dim, cl);
            // marker("MPI_Bcast");
            assertMpi(MPI_Bcast(cl, sizeof(Cluster), MPI_BYTE, MFOG_RANK_MAIN, MPI_COMM_WORLD));
            assertMpi(MPI_Bcast(cl->center, args->dim, MPI_DOUBLE, MFOG_RANK_MAIN, MPI_COMM_WORLD));
            if (model->size >= args->kParam) {
                fprintf(stderr, "model complete\n");
            }
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
        CircularExampleBuffer *fc;
        do {
            dest = (dest + 1) % args->mpiSize;
            fc = &args->flightController[dest];
            // while dest is root or dest has a full buffer
        } while (dest == MFOG_RANK_MAIN || isBufferFull(fc));
        //
        assertMpi(MPI_Pack(example, sizeof(Example), MPI_BYTE, buffer, bufferSize, &position, MPI_COMM_WORLD));
        assertMpi(MPI_Pack(example->val, args->dim, MPI_DOUBLE, buffer, bufferSize, &position, MPI_COMM_WORLD));
        assertMpi(MPI_Send(buffer, position, MPI_PACKED, dest, MFOG_TAG_EXAMPLE, MPI_COMM_WORLD));
        //
        // fprintf(stderr, "[sampler] push %d, %d\n", dest, example->id);
        example = CEB_enqueue(fc, example);
        // fprintf(stderr, "Sending to %d ex=%d buffer size=%d\n", dest, example->id, inFlightLen[dest]);
        //
    }
    example->label = MFOG_EOS_MARKER;
    int position = 0;
    assertMpi(MPI_Pack(example, sizeof(Example), MPI_BYTE, buffer, bufferSize, &position, MPI_COMM_WORLD));
    assertMpi(MPI_Pack(example->val, args->dim, MPI_DOUBLE, buffer, bufferSize, &position, MPI_COMM_WORLD));
    for (dest = 1; dest < args->mpiSize; dest++) {
        // fprintf(stderr, "[sampler %d] MFOG_EOS_MARKER to %d\n", args->mpiRank, dest);
        assertMpi(MPI_Send(buffer, position, MPI_PACKED, dest, MFOG_TAG_EXAMPLE, MPI_COMM_WORLD));
    }
    fprintf(stderr, "[sampler %d] %le seconds. At %s:%d\n", args->mpiRank, ((double)clock() - start) / 1000000.0, __FILE__, __LINE__);
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
    int streams = args->mpiSize - 1;
    int kParam = args->kParam, dim = args->dim, minExamplesPerCluster = args->minExamplesPerCluster;
    double precision = args->precision, radiusF = args->radiusF, noveltyF = args->noveltyF;
    Example *example = calloc(1, sizeof(Example));
    example->val = calloc(args->dim, sizeof(double));
    double *valuePtr = example->val;
    Example *ex = calloc(1, sizeof(Example));
    ex->val = calloc(args->dim, sizeof(double));
    //
    size_t noveltyDetectionTrigger = args->minExamplesPerCluster * args->kParam;
    size_t unknownsMaxSize = args->minExamplesPerCluster * args->kParam;
    Example *unknowns = calloc(unknownsMaxSize, sizeof(Example));
    size_t unknownsSize = 0, lastNDCheck = 0, id = 0;
    //
    Model *model = args->model;
    int mpiReturn;
    MPI_Status st;
    //
    while (streams > 0) {
        valuePtr = example->val;
        assertMpi(MPI_Recv(example, sizeof(Example), MPI_BYTE, MPI_ANY_SOURCE, MFOG_TAG_UNKNOWN, MPI_COMM_WORLD, &st));
        example->val = valuePtr;
        if (example->label == MFOG_EOS_MARKER) {
            // fprintf(stderr, "[detector %d] MFOG_EOS_MARKER %d\n", args->mpiRank, st.MPI_SOURCE);
            streams--;
            continue;
        }
        CircularExampleBuffer *fc = &args->flightController[st.MPI_SOURCE];
        Example copy = *example;
        // example = CEB_extract(fc, example);
        // fprintf(stderr, "[detector] pop %d, %d\n", st.MPI_SOURCE, example->id);
        example = CEB_dequeue(fc, example);
        example->label = copy.label;
        assertMsg(example->id == copy.id, "Out of order Exception%c", '.');
        //
        id = example->id > id ? example->id : id;
        printf("%10u,%s\n", example->id, printableLabel(example->label));
        fflush(stdout);
        if (example->label != MINAS_UNK_LABEL) continue;
        printf("Unknown: %10u", example->id);
        for (unsigned int d = 0; d < args->dim; d++)
            printf(", %le", example->val[d]);
        printf("\n");
        fflush(stdout);
        //
        unknowns[unknownsSize] = *example;
        unknownsSize++;
        example->val = calloc(dim, sizeof(double));
        if (unknownsSize >= unknownsMaxSize) {
            // TODO: garbage collect istead of realloc
            unknownsMaxSize *= 2;
            fprintf(stderr, "[detector %d] realloc unknowns to %lu "__FILE__":%d\n", args->mpiSize, unknownsMaxSize, __LINE__);
            unknowns = realloc(unknowns, unknownsMaxSize * sizeof(Example));
        }
        //
        if (unknownsSize >= noveltyDetectionTrigger && id - lastNDCheck > noveltyDetectionTrigger) {
            clock_t ndStart = clock();
            // marker("noveltyDetection");
            unsigned int prevSize = model->size;
            noveltyDetection(PARAMS, model, unknowns, unknownsSize);
            unsigned int nNewClusters = model->size - prevSize;
            clock_t ndEnd = clock();
            double ndTime = (ndEnd - ndStart) / 1000000.0;
            //
            for (size_t k = prevSize; k < model->size; k++) {
                Cluster *newCl = &model->clusters[k];
                printCluster(dim, newCl);
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
                double distance = nearestClusterVal(dim, &model->clusters[prevSize], nNewClusters, unknowns[ex].val, &nearest);
                assert(nearest != NULL);
                if (distance <= nearest->distanceMax) {
                    consumed++;
                    free(unknowns[ex].val);
                    continue;
                }
                distance = nearestClusterVal(dim, model->clusters, model->size - nNewClusters, unknowns[ex].val, &nearest);
                assert(nearest != NULL);
                if (distance <= nearest->distanceMax) {
                    reclassified++;
                    free(unknowns[ex].val);
                    continue;
                }
                if (unknowns[ex].id < lastNDCheck) {
                    discarted++;
                    free(unknowns[ex].val);
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
    Cluster cl;
    cl.label = MFOG_EOS_MARKER;
    assertMpi(MPI_Bcast(&cl, sizeof(Cluster), MPI_BYTE, MFOG_RANK_MAIN, MPI_COMM_WORLD));
    fprintf(stderr, "[detector %d] %le seconds. At %s:%d\n", args->mpiRank, ((double)clock() - start) / 1000000.0, __FILE__, __LINE__);
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
    args.mpiProcessorName = calloc(MPI_MAX_PROCESSOR_NAME + 1, sizeof(char));
    MPI_Get_processor_name(args.mpiProcessorName, &namelen);
    //
    assertErrno(sem_init(&args.modelReadySemaphore, 0, 0) >= 0, "Semaphore init fail%c.", '.', /**/);
    assertErrno(pthread_mutex_init(&args.modelMutex, NULL) >= 0, "Mutex init fail%c.", '.', /**/);
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
            argv[0], PARAMS, args.mpiProcessorName, args.mpiRank, args.mpiSize);
        printf("#pointId,label\n");
        fflush(stdout);
        args.flightController = calloc(args.mpiSize, sizeof(CircularExampleBuffer));
        for (size_t i = 0; i < args.mpiSize; i++) {
            CEB_init(&args.flightController[i], 1000, dim);
        }

        pthread_t detector_t, sampler_t;
        int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg);
        assertErrno(pthread_create(&sampler_t, NULL, sampler, (void *)&args) == 0, "Thread creation fail%c.", '.', MPI_Finalize());
        assertErrno(pthread_create(&detector_t, NULL, detector, (void *)&args) == 0, "Thread creation fail%c.", '.', MPI_Finalize());
        //
        assertErrno(pthread_join(sampler_t, (void **)&result) == 0, "Thread join fail%c.", '.', MPI_Finalize());
        assertErrno(pthread_join(detector_t, (void **)&result) == 0, "Thread join fail%c.", '.', MPI_Finalize());
        //
        CEB_destroy(args.flightController);
    } else {
        pthread_t classifier_t, m_receiver_t;
        assertErrno(pthread_create(&classifier_t, NULL, classifier, (void *)&args) == 0, "Thread creation fail%c.", '.', MPI_Finalize());
        assertErrno(pthread_create(&m_receiver_t, NULL, m_receiver, (void *)&args) == 0, "Thread creation fail%c.", '.', MPI_Finalize());
        //
        assertErrno(pthread_join(classifier_t, (void **)&result) == 0, "Thread join fail%c.", '.', MPI_Finalize());
        assertErrno(pthread_join(m_receiver_t, (void **)&result) == 0, "Thread join fail%c.", '.', MPI_Finalize());
    }
    fprintf(stderr, "[%s] %le seconds. At %s:%d\n", argv[0], ((double)clock() - start) / 1000000.0, __FILE__, __LINE__);
    free(args.model);
    sem_destroy(&args.modelReadySemaphore);
    return MPI_Finalize();
}
