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

typedef struct {
    Example *buff;
    int size, head, tail, len;
    // Semaforo de dados para o emissor
    sem_t senderSem;
} CircularExampleBuffer;

struct ThreadArgs_s {
    int kParam, dim, minExamplesPerCluster;
    double precision, radiusF, noveltyF;
    // mpi stuff
    int mpiRank, mpiSize;
    char *mpiProcessorName;
    CircularExampleBuffer unkBuffer;
    Model *model;
    pthread_mutex_t modelMutex;
};
typedef struct ThreadArgs_s ThreadArgs;

void *classifier(void *arg) {
    ThreadArgs *args = arg;
    Example example;
    example.val = calloc(args->dim, sizeof(double));
    double *valuePtr = example.val;
    //
    Model *model = args->model;
    //
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
        assertMpi(MPI_Unpack(buffer, bufferSize, &position, &example, sizeof(Example), MPI_BYTE, MPI_COMM_WORLD));
        // if (example->id < 0) return 0;
        assertMpi(MPI_Unpack(buffer, bufferSize, &position, valuePtr, args->dim, MPI_DOUBLE, MPI_COMM_WORLD));
        example.val = valuePtr;
        (*inputLine)++;
        //
        pthread_mutex_lock(&args->modelMutex);
        identify(args->kParam, args->dim, args->precision, args->radiusF, model, &example, &match);
        pthread_mutex_unlock(&args->modelMutex);
        printf("%10u,%s\n", example.id, printableLabel(match.label));
        //
        if (match.label != UNK_LABEL) continue;
        printf("Unknown: %10u", example.id);
        for (unsigned int d = 0; d < args->dim; d++)
            printf(", %le", example.val[d]);
        printf("\n");
        // the buffer is full when the head pointer is one less than the tail pointer.
        while (args->unkBuffer.len == args->unkBuffer.size) {
            sem_wait(&args->unkBuffer.senderSem);
        }
        Example swp = args->unkBuffer.buff[args->unkBuffer.head];
        args->unkBuffer.buff[args->unkBuffer.head] = example;
        example = swp;
        args->unkBuffer.head++;
        args->unkBuffer.head %= args->unkBuffer.size;
        args->unkBuffer.len++;
        valuePtr = example.val;
        sem_post(&args->unkBuffer.senderSem);
    }
    return inputLine;
}
void *u_sender(void *arg) {
    ThreadArgs *args = arg;
    Example example;
    example.val = calloc(args->dim, sizeof(double));
    //
    int exampleBufferLen, valueBufferLen;
    MPI_Pack_size(sizeof(Example), MPI_BYTE, MPI_COMM_WORLD, &exampleBufferLen);
    MPI_Pack_size(args->dim, MPI_DOUBLE, MPI_COMM_WORLD, &valueBufferLen);
    int bufferSize = exampleBufferLen + valueBufferLen + MPI_BSEND_OVERHEAD;
    int *buffer = (int *)malloc(bufferSize);
    int mpiReturn;

    while(1) {
        // espera threshold (semaforo)
        if (args->unkBuffer.len == 0) {
            sem_wait(&args->unkBuffer.senderSem);
        }
        Example swp = args->unkBuffer.buff[args->unkBuffer.tail];
        args->unkBuffer.buff[args->unkBuffer.tail] = example;
        example = swp;
        args->unkBuffer.tail++;
        args->unkBuffer.tail %= args->unkBuffer.size;
        if (args->unkBuffer.len == args->unkBuffer.size) {
            sem_post(&args->unkBuffer.senderSem);
        }
        args->unkBuffer.len--;

        int position = 0;
        assertMpi(MPI_Pack(&example, sizeof(Example), MPI_BYTE, buffer, bufferSize, &position, MPI_COMM_WORLD));
        assertMpi(MPI_Pack(example.val, args->dim, MPI_DOUBLE, buffer, bufferSize, &position, MPI_COMM_WORLD));
        assertMpi(MPI_Send(buffer, position, MPI_PACKED, MFOG_RANK_MAIN, MFOG_TAG_UNKNOWN, MPI_COMM_WORLD));
    }
}

/**
 * @brief In rank N != 0, this thread receives model updates.
 * 
 * @param arg ThreadArgs
 * @return void* 
 */
void *m_receiver(void *arg) {
    ThreadArgs *args = arg;
    Cluster *cl = calloc(1, sizeof(Cluster));
    cl->center = calloc(args->dim, sizeof(double));
    double *valuePtr = cl->center;
    int clusterBufferLen, valueBufferLen;
    MPI_Pack_size(sizeof(Cluster), MPI_BYTE, MPI_COMM_WORLD, &clusterBufferLen);
    MPI_Pack_size(args->dim, MPI_DOUBLE, MPI_COMM_WORLD, &valueBufferLen);
    int bufferSize = clusterBufferLen + valueBufferLen + MPI_BSEND_OVERHEAD;
    int *buffer = (int *)malloc(bufferSize);
    Model *model = args->model;
    fprintf(stderr, "m_receiver at %d/%d\n", args->mpiRank, args->mpiSize);
    while (1) {
        int position = 0, mpiReturn;
        // marker("MPI_Bcast");
        // assertMpi(MPI_Bcast(buffer, position, MPI_PACKED, MFOG_RANK_MAIN, MPI_COMM_WORLD));
        // assertMpi(MPI_Unpack(buffer, bufferSize, &position, cl, sizeof(Cluster), MPI_BYTE, MPI_COMM_WORLD));
        // assertMpi(MPI_Unpack(buffer, bufferSize, &position, valuePtr, args->dim, MPI_DOUBLE, MPI_COMM_WORLD));
        // assertMsg(position < bufferSize, "Buffer sizing error, got %d", position);
        marker("MPI_Bcast");
        assertMpi(MPI_Bcast(cl, sizeof(Cluster), MPI_BYTE, MFOG_RANK_MAIN, MPI_COMM_WORLD));
        assertMpi(MPI_Bcast(valuePtr, args->dim, MPI_DOUBLE, MFOG_RANK_MAIN, MPI_COMM_WORLD));
        cl->center = valuePtr;

        // printCluster(args->dim, cl);
        assert(cl->id == model->size);
        pthread_mutex_lock(&args->modelMutex);
        if (model->size > 0 && model->size % args->kParam == 0) {
            fprintf(stderr, "realloc model %d\n", model->size);
            model->clusters = realloc(model->clusters, (model->size + args->kParam) * sizeof(Cluster));
        }
        model->clusters[model->size] = *cl;
        model->size++;
        pthread_mutex_unlock(&args->modelMutex);
        // new center array, prep for next cluster
        cl->center = calloc(args->dim, sizeof(double));
        valuePtr = cl->center;
    }
    return model;
}

/**
 * @brief In rank 0, sampler thread takes stdin and distributes Examples and (init) Clusters.
 * 
 * @param arg ThreadArgs
 * @return void* ptr to int consumed lines
 */
void *sampler(void *arg) {
    ThreadArgs *args = arg;
    unsigned int id = 0;
    Example example;
    example.val = calloc(args->dim, sizeof(double));
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
    int mpiReturn, dest = 1;
    int *buffer = (int *)malloc(bufferSize);
    MPI_Buffer_attach(buffer, bufferSize);
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
            //
            // assertMpi(MPI_Pack(cl, sizeof(Cluster), MPI_BYTE, buffer, bufferSize, &position, MPI_COMM_WORLD));
            // assertMpi(MPI_Pack(cl->center, args->dim, MPI_DOUBLE, buffer, bufferSize, &position, MPI_COMM_WORLD));
            // assertMsg(position < bufferSize, "Buffer sizing error, got %d", position);
            // marker("MPI_Bcast");
            // assertMpi(MPI_Bcast(buffer, position, MPI_PACKED, MFOG_RANK_MAIN, MPI_COMM_WORLD));
            //
            // marker("MPI_Bcast");
            assertMpi(MPI_Bcast(cl, sizeof(Cluster), MPI_BYTE, MFOG_RANK_MAIN, MPI_COMM_WORLD));
            assertMpi(MPI_Bcast(cl->center, args->dim, MPI_DOUBLE, MFOG_RANK_MAIN, MPI_COMM_WORLD));
            if (model->size >= args->kParam) {
                fprintf(stderr, "model complete\n");
            }
            continue;
        }
        for (size_t d = 0; d < args->dim; d++) {
            assert(sscanf(&lineptr[readTot], "%lf,%n", &example.val[d], &readCur));
            readTot += readCur;
        }
        // ignore class
        example.id = id;
        id++;
        //
        assertMpi(MPI_Pack(&example, sizeof(Example), MPI_BYTE, buffer, bufferSize, &position, MPI_COMM_WORLD));
        assertMpi(MPI_Pack(example.val, args->dim, MPI_DOUBLE, buffer, bufferSize, &position, MPI_COMM_WORLD));
        assertMpi(MPI_Send(buffer, position, MPI_PACKED, dest, MFOG_TAG_EXAMPLE, MPI_COMM_WORLD));
        dest = (dest + 1) % args->mpiSize;
        if (dest == MFOG_RANK_MAIN) dest++;
    }
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
    ThreadArgs *args = arg;
    int kParam = args->kParam, dim = args->dim, minExamplesPerCluster = args->minExamplesPerCluster;
    double precision = args->precision, radiusF = args->radiusF, noveltyF = args->noveltyF;
    Example example;
    example.val = calloc(args->dim, sizeof(double));
    double *valuePtr = example.val;
    //
    size_t noveltyDetectionTrigger = args->minExamplesPerCluster * args->kParam;
    size_t unknownsMaxSize = args->minExamplesPerCluster * args->kParam;
    Example *unknowns = calloc(unknownsMaxSize, sizeof(Example));
    size_t unknownsSize = 0, lastNDCheck = 0, id = 0;
    //
    Model *model = args->model;
    //
    // determina espaço necessário para empacotamento dos dados no transmissor e recebimento no nó tratador
    int clusterBufferLen, exampleBufferLen, valueBufferLen;
    int mpiReturn;
    assertMpi(MPI_Pack_size(sizeof(Cluster), MPI_BYTE, MPI_COMM_WORLD, &clusterBufferLen));
    assertMpi(MPI_Pack_size(sizeof(Example), MPI_BYTE, MPI_COMM_WORLD, &exampleBufferLen));
    assertMpi(MPI_Pack_size(args->dim, MPI_DOUBLE, MPI_COMM_WORLD, &valueBufferLen));
    int bufferSize = clusterBufferLen + exampleBufferLen + valueBufferLen + 2 * MPI_BSEND_OVERHEAD;
    int *buffer = (int *)malloc(bufferSize);
    while (1) {
        assertMpi(MPI_Recv(buffer, bufferSize, MPI_PACKED, MPI_ANY_SOURCE, MFOG_TAG_UNKNOWN, MPI_COMM_WORLD, MPI_STATUS_IGNORE));
        int position = 0;
        assertMpi(MPI_Unpack(buffer, bufferSize, &position, &example, sizeof(Example), MPI_BYTE, MPI_COMM_WORLD));
        // if (example->id < 0) return 0;
        assertMpi(MPI_Unpack(buffer, bufferSize, &position, valuePtr, args->dim, MPI_DOUBLE, MPI_COMM_WORLD));
        example.val = valuePtr;
        //
        unknowns[unknownsSize] = example;
        unknownsSize++;
        example.val = calloc(dim, sizeof(double));
        if (unknownsSize >= unknownsMaxSize) {
            unknownsMaxSize *= 2;
            unknowns = realloc(unknowns, unknownsMaxSize * sizeof(Example));
        }
        //
        if (unknownsSize % noveltyDetectionTrigger == 0 && id - lastNDCheck > noveltyDetectionTrigger) {
            lastNDCheck = id;
            unsigned int prevSize = model->size;
            noveltyDetection(PARAMS, model, unknowns, unknownsSize);
            unsigned int nNewClusters = model->size - prevSize;
            //
            size_t reclassified = 0;
            for (size_t ex = 0; ex < unknownsSize; ex++) {
                // compress
                unknowns[ex - reclassified] = unknowns[ex];
                Cluster *nearest;
                double distance = nearestClusterVal(dim, &model->clusters[prevSize], nNewClusters, unknowns[ex].val, &nearest);
                assert(nearest != NULL);
                if (distance <= nearest->distanceMax) {
                    reclassified++;
                }
            }
            fprintf(stderr, "Reclassified %lu\n", reclassified);
            unknownsSize -= reclassified;
            //
            for (size_t k = prevSize; k < model->size; k++) {
                Cluster *newCl = &model->clusters[k];
                printCluster(dim, newCl);
                // position = 0;
                // assertMpi(MPI_Pack(newCl, sizeof(Cluster), MPI_BYTE, buffer, bufferSize, &position, MPI_COMM_WORLD));
                // assertMpi(MPI_Pack(newCl->center, args->dim, MPI_DOUBLE, buffer, bufferSize, &position, MPI_COMM_WORLD));
                // assertMsg(position < bufferSize, "Buffer sizing error, got %d", position);
                // assertMpi(MPI_Bcast(buffer, position, MPI_PACKED, MFOG_RANK_MAIN, MPI_COMM_WORLD));
                assertMpi(MPI_Bcast(newCl, sizeof(Cluster), MPI_BYTE, MFOG_RANK_MAIN, MPI_COMM_WORLD));
                assertMpi(MPI_Bcast(newCl->center, args->dim, MPI_DOUBLE, MFOG_RANK_MAIN, MPI_COMM_WORLD));
            }
        }
    }
}

int main(int argc, char const *argv[]) {
    // int result, count, i, tag;
    srand(getpid());

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
    int provided;
    int result = MPI_Init_thread(&argc, (char***) &argv, MPI_THREAD_MULTIPLE, &provided);
    if (result != MPI_SUCCESS || provided != MPI_THREAD_MULTIPLE) {
        MPI_Abort(MPI_COMM_WORLD, result);
        fail("Erro iniciando programa MPI #%d.\n", result);
    }
    MPI_Comm_rank(MPI_COMM_WORLD, &args.mpiRank);
    MPI_Comm_size(MPI_COMM_WORLD, &args.mpiSize);
    assertMsg(args.mpiSize > 1, "This is a multi-process program, got only %d process.", args.mpiSize);
    int namelen;
    args.mpiProcessorName = calloc(MPI_MAX_PROCESSOR_NAME + 1, sizeof(char));
    MPI_Get_processor_name(args.mpiProcessorName, &namelen);
    //
    // if (args.mpiRank == MFOG_RANK_MAIN)
    fprintf(stderr,
            "%s; kParam=%d; dim=%d; precision=%le; radiusF=%le; minExamplesPerCluster=%d; noveltyF=%le;\t"
            "Hello from %s, rank %d/%d\n",
            argv[0], PARAMS, args.mpiProcessorName, args.mpiRank, args.mpiSize);
    //
    assertErrno(sem_init(&args.unkBuffer.senderSem, 0, 0) >= 0, "Semaphore init fail%c.", '.', /**/);
    assertErrno(pthread_mutex_init(&args.modelMutex, NULL) >= 0, "Mutex init fail%c.", '.', /**/);
    //
    args.model = calloc(1, sizeof(Model));
    args.model->size = 0;
    args.model->nextLabel = '\0';
    args.model->clusters = calloc(kParam, sizeof(Cluster));
    args.unkBuffer.head = 0;
    args.unkBuffer.tail = 0;
    args.unkBuffer.len = 0;
    args.unkBuffer.size = 20;
    args.unkBuffer.buff = calloc(args.unkBuffer.size, sizeof(Example));
    //
    if (args.mpiRank == MFOG_RANK_MAIN) {
        printf("#pointId,label\n");

        pthread_t detector_t, sampler_t;
        // int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg);
        assertErrno(pthread_create(&sampler_t, NULL, sampler, (void *)&args) == 0, "Thread creation fail%c.", '.', MPI_Finalize());
        // assertErrno(pthread_create(&detector_t, NULL, detector, (void *)&args) == 0, "Thread creation fail%c.", '.', MPI_Finalize());
        //
        assertErrno(pthread_join(sampler_t, (void **)&result) == 0, "Thread join fail%c.", '.', MPI_Finalize());
        // assertErrno(pthread_join(detector_t, (void **)&result) == 0, "Thread join fail%c.", '.', MPI_Finalize());
    } else {
        pthread_t classifier_t, u_sender_t, m_receiver_t;
        assertErrno(pthread_create(&classifier_t, NULL, classifier, (void *)&args) == 0, "Thread creation fail%c.", '.', MPI_Finalize());
        // assertErrno(pthread_create(&u_sender_t, NULL, u_sender, (void *)&args) == 0, "Thread creation fail%c.", '.', MPI_Finalize());
        assertErrno(pthread_create(&m_receiver_t, NULL, m_receiver, (void *)&args) == 0, "Thread creation fail%c.", '.', MPI_Finalize());
        //
        assertErrno(pthread_join(classifier_t, (void **)&result) == 0, "Thread join fail%c.", '.', MPI_Finalize());
        // assertErrno(pthread_join(u_sender_t, (void **)&result) == 0, "Thread join fail%c.", '.', MPI_Finalize());
        assertErrno(pthread_join(m_receiver_t, (void **)&result) == 0, "Thread join fail%c.", '.', MPI_Finalize());
    }
    free(args.model);
    sem_destroy(&args.unkBuffer.senderSem);
    return EXIT_SUCCESS;
}
