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
    CircularExampleBuffer *unkBuffer, *flightController;
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
        if (example->label == MFOG_EOS_MARKER) {
            // forward end of stream marker
            // marker("Classifier EOS");
            assertMpi(MPI_Send(buffer, position, MPI_PACKED, MFOG_RANK_MAIN, MFOG_TAG_UNKNOWN, MPI_COMM_WORLD));
            example->val = valuePtr;
            example = CEB_push(example, args->unkBuffer);
            break;
        }
        assertMpi(MPI_Unpack(buffer, bufferSize, &position, valuePtr, args->dim, MPI_DOUBLE, MPI_COMM_WORLD));
        example->val = valuePtr;
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
        example = CEB_push(example, args->unkBuffer);
        //
        // position = 0;
        // assertMpi(MPI_Pack(&example, sizeof(Example), MPI_BYTE, buffer, bufferSize, &position, MPI_COMM_WORLD));
        // assertMpi(MPI_Pack(example->val, args->dim, MPI_DOUBLE, buffer, bufferSize, &position, MPI_COMM_WORLD));
        // assertMpi(MPI_Send(buffer, position, MPI_PACKED, MFOG_RANK_MAIN, MFOG_TAG_UNKNOWN, MPI_COMM_WORLD));
    }
    fprintf(stderr, "[classifier %d] %le seconds. At %s:%d\n", args->mpiRank, ((double)clock() - start) / 1000000.0, __FILE__, __LINE__);
    free(buffer);
    // free(example->val);
    free(example);
    return inputLine;
}
void *u_sender(void *arg) {
    clock_t start = clock();
    ThreadArgs *args = arg;
    fprintf(stderr, "[u_sender %d]\n", args->mpiRank);
    size_t *inputLine = calloc(1, sizeof(size_t));
    Example *example = calloc(1, sizeof(Example));
    example->val = calloc(args->dim, sizeof(double));
    int mpiReturn;

    while(1) {
        example = CEB_pop(example, args->unkBuffer);
        if (example->label == MFOG_EOS_MARKER) break;
        (*inputLine)++;
        assertMpi(MPI_Send(example, sizeof(Example), MPI_BYTE, MFOG_RANK_MAIN, MFOG_TAG_UNKNOWN, MPI_COMM_WORLD));
    }
    fprintf(stderr, "[u_sender %d] %le seconds. At %s:%d\n", args->mpiRank, ((double)clock() - start) / 1000000.0, __FILE__, __LINE__);
    free(example->val); // don't free, last example (MFOG_EOS_MARKER) does not have a valid value
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
        if (cl->label == MFOG_EOS_MARKER) break;
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
        if (model->size == args->kParam) {
            fprintf(stderr, "model complete\n");
            sem_post(&args->modelReadySemaphore);
        }
        // if (model->size > args->kParam) {
        //     marker("extra cluster");
        // }
        // new center array, prep for next cluster
        cl->center = calloc(args->dim, sizeof(double));
        valuePtr = cl->center;
    }
    fprintf(stderr, "[m_receiver %d] %le seconds. At %s:%d\n", args->mpiRank, ((double)clock() - start) / 1000000.0, __FILE__, __LINE__);
    return model;
}

int collector(ThreadArgs *args, Example **inFlight, int *inFlightHead, int *inFlightTail, int *inFlightLen, int inFlightSize, int destTries) {
    MPI_Status st;
    int hasMessage, mpiReturn;
    Example *example = calloc(1, sizeof(Example));
    if (destTries > 0) {
        assertMpi(MPI_Iprobe(MPI_ANY_SOURCE, MFOG_TAG_UNKNOWN, MPI_COMM_WORLD, &hasMessage, &st));
    } else {
        hasMessage = 1;
        // marker("block")
        assertMpi(MPI_Recv(example, sizeof(Example), MPI_BYTE, MPI_ANY_SOURCE, MFOG_TAG_UNKNOWN, MPI_COMM_WORLD, &st));
    }
    if (hasMessage) {
        Example *data = inFlight[st.MPI_SOURCE];
        int tail = inFlightTail[st.MPI_SOURCE];
        int head = inFlightHead[st.MPI_SOURCE];
        int len = inFlightLen[st.MPI_SOURCE];
        if (destTries > 0) {
            assertMpi(MPI_Recv(example, sizeof(Example), MPI_BYTE, st.MPI_SOURCE, MFOG_TAG_UNKNOWN, MPI_COMM_WORLD, MPI_STATUS_IGNORE));
        }
        if (example->label == MFOG_EOS_MARKER) {
            marker("MFOG_EOS_MARKER collector");
            example = CEB_push(example, args->unkBuffer);
            return 1;
        }
        // fprintf(stderr, "from %d got ex=%d with buffer size=%d\n", st.MPI_SOURCE, example->id, len);
        //
        if (len == 0)
            fprintf(stderr, "buff shouldn't be empty but got i=%d sending me ex=%d with buffer size=%d\n", st.MPI_SOURCE, example->id, len);
        if (data[tail].id != example->id) {
            for (size_t i = (tail + 1) % inFlightSize; i != ((head+inFlightSize+1)%inFlightSize); i = ((i+1)%inFlightSize)) {
                if (data[i].id == example->id) {
                    // swap tail and i
                    Example e = data[i];
                    data[i] = data[tail];
                    data[tail] = e;
                    break;
                }
            }
        }
        // easy
        example->val = data[tail].val;
        data[tail].val = calloc(args->dim, sizeof(double));
        inFlightTail[st.MPI_SOURCE] = (inFlightTail[st.MPI_SOURCE] + 1) % inFlightSize;
        inFlightLen[st.MPI_SOURCE]--;
        example = CEB_push(example, args->unkBuffer);
    }
    return 0;
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
    // MPI_Buffer_attach(buffer, bufferSize);
    //
    int *inFlightHead = calloc(args->mpiSize, sizeof(int));
    int *inFlightTail = calloc(args->mpiSize, sizeof(int));
    int *inFlightLen = calloc(args->mpiSize, sizeof(int));
    int inFlightSize = 500;
    Example **inFlight = calloc(args->mpiSize, sizeof(Example*));
    for (size_t i = 0; i < args->mpiSize; i++) {
        inFlightHead[i] = 0;
        inFlightTail[i] = 0;
        inFlightLen[i] = 0;
        inFlight[i] = calloc(inFlightSize, sizeof(Example));
        for (size_t j = 0; j < inFlightSize; j++) {
            inFlight[i][j].val = calloc(args->dim, sizeof(double));
        }
    }
    //
    fprintf(stderr, "Taking test stream from stdin\n");
    fprintf(stderr, "sampler at %d/%d\n", args->mpiRank, args->mpiSize);
    int streams = args->mpiSize - 1;
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
        int destTries = args->mpiSize;
        do {
            dest = (dest + 1) % args->mpiSize;
            //
            streams -= collector(args, inFlight, inFlightHead, inFlightTail, inFlightLen, inFlightSize, destTries);
            destTries--;
            // while dest is root or dest has a full buffer
        } while (dest == MFOG_RANK_MAIN || inFlightLen[dest] == (inFlightSize - 1));
        //
        assertMpi(MPI_Pack(example, sizeof(Example), MPI_BYTE, buffer, bufferSize, &position, MPI_COMM_WORLD));
        assertMpi(MPI_Pack(example->val, args->dim, MPI_DOUBLE, buffer, bufferSize, &position, MPI_COMM_WORLD));
        assertMpi(MPI_Send(buffer, position, MPI_PACKED, dest, MFOG_TAG_EXAMPLE, MPI_COMM_WORLD));
        //
        Example* data = inFlight[dest];
        int head = inFlightHead[dest];
        data[head].id = example->id;
        double *sw = data[head].val;
        data[head].val = example->val;
        example->val = sw;
        inFlightHead[dest] = (head + 1) % inFlightSize;
        inFlightLen[dest]++;
        // fprintf(stderr, "Sending to %d ex=%d buffer size=%d\n", dest, example->id, inFlightLen[dest]);
        //
    }
    example->label = MFOG_EOS_MARKER;
    int position = 0;
    assertMpi(MPI_Pack(example, sizeof(Example), MPI_BYTE, buffer, bufferSize, &position, MPI_COMM_WORLD));
    assertMpi(MPI_Pack(example->val, args->dim, MPI_DOUBLE, buffer, bufferSize, &position, MPI_COMM_WORLD));
    for (dest = 1; dest < args->mpiSize; dest++) {
        assertMpi(MPI_Send(buffer, position, MPI_PACKED, dest, MFOG_TAG_EXAMPLE, MPI_COMM_WORLD));
    }
    while (streams > 0) {
        streams -= collector(args, inFlight, inFlightHead, inFlightTail, inFlightLen, inFlightSize, 0);
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
    // example->val = calloc(args->dim, sizeof(double));
    // double *valuePtr = example->val;
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
    //
    while (streams > 0) {
        // assertMpi(MPI_Recv(buffer, bufferSize, MPI_PACKED, MPI_ANY_SOURCE, MFOG_TAG_UNKNOWN, MPI_COMM_WORLD, MPI_STATUS_IGNORE));
        // int position = 0;
        // marker("MPI_Unpack");
        // assertMpi(MPI_Unpack(buffer, bufferSize, &position, example, sizeof(Example), MPI_BYTE, MPI_COMM_WORLD));
        // if (example->id < 0) return 0;
        // assertMpi(MPI_Unpack(buffer, bufferSize, &position, valuePtr, args->dim, MPI_DOUBLE, MPI_COMM_WORLD));
        // example->val = valuePtr;
        //
        example = CEB_pop(example, args->unkBuffer);
        if (example->label == MFOG_EOS_MARKER) break;
        //
        id = example->id > id ? example->id : id;
        printf("%10u,%s\n", example->id, printableLabel(example->label));
        fflush(stdout);
        if (example->label != UNK_LABEL) continue;
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
            fprintf(stderr, "realloc unknowns to %lu "__FILE__":%d\n", unknownsMaxSize, __LINE__);
            unknowns = realloc(unknowns, unknownsMaxSize * sizeof(Example));
        }
        //
        if (unknownsSize >= noveltyDetectionTrigger && id - lastNDCheck > noveltyDetectionTrigger) {
            // marker("noveltyDetection");
            unsigned int prevSize = model->size;
            noveltyDetection(PARAMS, model, unknowns, unknownsSize);
            unsigned int nNewClusters = model->size - prevSize;
            //
            for (size_t k = prevSize; k < model->size; k++) {
                Cluster *newCl = &model->clusters[k];
                // printCluster(dim, newCl);
                assertMpi(MPI_Bcast(newCl, sizeof(Cluster), MPI_BYTE, MFOG_RANK_MAIN, MPI_COMM_WORLD));
                assertMpi(MPI_Bcast(newCl->center, args->dim, MPI_DOUBLE, MFOG_RANK_MAIN, MPI_COMM_WORLD));
            }
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
            fprintf(stderr, "ND consumed %lu, reclassified %lu, discarted %lu\n", consumed, reclassified, discarted);
            unknownsSize -= (discarted + consumed + reclassified);
            lastNDCheck = id;
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
        args.unkBuffer = CEB_create(1000, dim);

        pthread_t detector_t, sampler_t;
        int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg);
        assertErrno(pthread_create(&sampler_t, NULL, sampler, (void *)&args) == 0, "Thread creation fail%c.", '.', MPI_Finalize());
        assertErrno(pthread_create(&detector_t, NULL, detector, (void *)&args) == 0, "Thread creation fail%c.", '.', MPI_Finalize());
        //
        assertErrno(pthread_join(sampler_t, (void **)&result) == 0, "Thread join fail%c.", '.', MPI_Finalize());
        assertErrno(pthread_join(detector_t, (void **)&result) == 0, "Thread join fail%c.", '.', MPI_Finalize());
    } else {
        args.unkBuffer = CEB_create(100, dim);
        pthread_t classifier_t, u_sender_t, m_receiver_t;
        assertErrno(pthread_create(&classifier_t, NULL, classifier, (void *)&args) == 0, "Thread creation fail%c.", '.', MPI_Finalize());
        assertErrno(pthread_create(&u_sender_t, NULL, u_sender, (void *)&args) == 0, "Thread creation fail%c.", '.', MPI_Finalize());
        assertErrno(pthread_create(&m_receiver_t, NULL, m_receiver, (void *)&args) == 0, "Thread creation fail%c.", '.', MPI_Finalize());
        //
        assertErrno(pthread_join(classifier_t, (void **)&result) == 0, "Thread join fail%c.", '.', MPI_Finalize());
        assertErrno(pthread_join(u_sender_t, (void **)&result) == 0, "Thread join fail%c.", '.', MPI_Finalize());
        assertErrno(pthread_join(m_receiver_t, (void **)&result) == 0, "Thread join fail%c.", '.', MPI_Finalize());
    }
    fprintf(stderr, "[%s] %le seconds. At %s:%d\n", argv[0], ((double)clock() - start) / 1000000.0, __FILE__, __LINE__);
    free(args.model);
    CEB_destroy(args.unkBuffer);
    sem_destroy(&args.modelReadySemaphore);
    return MPI_Finalize();
}
