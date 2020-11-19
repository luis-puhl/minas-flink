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

struct ThreadArgs_s {
    int kParam, dim, minExamplesPerCluster;
    double precision, radiusF, noveltyF;
    // mpi stuff
    int mpiRank, mpiSize;
    char *mpiProcessorName;
    CircularExampleBuffer *unkBuffer;
    Model *model;
    pthread_mutex_t modelMutex;
    sem_t modelReadySemaphore;
};
typedef struct ThreadArgs_s ThreadArgs;

void *classifier(void *arg) {
    clock_t start = clock();
    ThreadArgs *args = arg;
    Example *example = calloc(1, sizeof(Example));
    example->val = calloc(args->dim, sizeof(double));
    double *valuePtr = example->val;
    fprintf(stderr, "classifier %d %p\n", args->mpiRank, example->val);
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
            marker("Classifier EOS");
            assertMpi(MPI_Send(buffer, position, MPI_PACKED, MFOG_RANK_MAIN, MFOG_TAG_UNKNOWN, MPI_COMM_WORLD));
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
        // un-buffer stdout https://stackoverflow.com/a/5310506/1774806
        // printf("%10u,%s\n", example->id, printableLabel(example->label));
        // fprintf(stderr, "-%p-\n", example->val);
        // fflush(stdout);
        example = CEB_push(example, args->unkBuffer);
        // if (example->label != UNK_LABEL) continue;
        // printf("Unknown: %10u", example->id);
        // for (unsigned int d = 0; d < args->dim; d++)
        //     printf(", %le", example->val[d]);
        // printf("\n");
        // fflush(stdout);
        //
        // position = 0;
        // assertMpi(MPI_Pack(&example, sizeof(Example), MPI_BYTE, buffer, bufferSize, &position, MPI_COMM_WORLD));
        // assertMpi(MPI_Pack(example->val, args->dim, MPI_DOUBLE, buffer, bufferSize, &position, MPI_COMM_WORLD));
        // assertMpi(MPI_Send(buffer, position, MPI_PACKED, MFOG_RANK_MAIN, MFOG_TAG_UNKNOWN, MPI_COMM_WORLD));
    }
    fprintf(stderr, "[%s] %le seconds. At %s:%d\n", "classifier", ((double)clock() - start) / 1000000.0, __FILE__, __LINE__);
    free(buffer);
    free(example->val);
    free(example);
    return inputLine;
}
void *u_sender(void *arg) {
    clock_t start = clock();
    ThreadArgs *args = arg;
    size_t *inputLine = calloc(1, sizeof(size_t));
    Example *example = calloc(1, sizeof(Example));
    example->val = calloc(args->dim, sizeof(double));
    //
    int exampleBufferLen, valueBufferLen;
    MPI_Pack_size(sizeof(Example), MPI_BYTE, MPI_COMM_WORLD, &exampleBufferLen);
    MPI_Pack_size(args->dim, MPI_DOUBLE, MPI_COMM_WORLD, &valueBufferLen);
    int bufferSize = exampleBufferLen + valueBufferLen + MPI_BSEND_OVERHEAD;
    int *buffer = (int *)malloc(bufferSize);
    int mpiReturn;

    while(1) {
        example = CEB_pop(example, args->unkBuffer);
        if (example->label == MFOG_EOS_MARKER) break;
        // fprintf(stderr, "U-sender Example(%u, %c, %le)\n", example->id, example->label, example->val[0]);
        (*inputLine)++;
        int position = 0;
        // fprintf(stderr, "sender %d %p\n", args->mpiRank, example->val);
        assertMpi(MPI_Pack(example, sizeof(Example), MPI_BYTE, buffer, bufferSize, &position, MPI_COMM_WORLD));
        // assertMpi(MPI_Pack(example->val, args->dim, MPI_DOUBLE, buffer, bufferSize, &position, MPI_COMM_WORLD));
        assertMpi(MPI_Send(buffer, position, MPI_PACKED, MFOG_RANK_MAIN, MFOG_TAG_UNKNOWN, MPI_COMM_WORLD));
    }
    fprintf(stderr, "[%s] %le seconds. At %s:%d\n", "u_sender", ((double)clock() - start) / 1000000.0, __FILE__, __LINE__);
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
    fprintf(stderr, "[%s] %le seconds. At %s:%d\n", "m_receiver", ((double)clock() - start) / 1000000.0, __FILE__, __LINE__);
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
    Example **inFlight = calloc(args->mpiSize, sizeof(Example*));
    for (size_t i = 0; i < args->mpiSize; i++) {
        inFlightHead[i] = 0;
        inFlightTail[i] = 0;
        inFlight[i] = calloc(args->kParam, sizeof(Example));
        for (size_t j = 0; j < args->kParam; j++) {
            inFlight[i][j].val = calloc(args->dim, sizeof(double));
        }
    }
    //
    fprintf(stderr, "Taking test stream from stdin\n");
    fprintf(stderr, "sampler at %d/%d\n", args->mpiRank, args->mpiSize);
    double *sw;
    MPI_Status st;
    int hasMessage;
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
        do {
            dest = (dest + 1) % args->mpiSize;
            MPI_Iprobe(MPI_ANY_SOURCE, MFOG_TAG_UNKNOWN, MPI_COMM_WORLD, &hasMessage, &st);
            if (hasMessage) {
                assertMpi(MPI_Recv(example, sizeof(Example), MPI_BYTE, st.MPI_SOURCE, MFOG_TAG_UNKNOWN, MPI_COMM_WORLD, MPI_STATUS_IGNORE));
                Example* data = inFlight[st.MPI_SOURCE];
                int tail = inFlightTail[st.MPI_SOURCE];
                int head = inFlightHead[st.MPI_SOURCE];
                if (data[tail].id == example->id) {
                    // easy
                    sw = example->val;
                    example->val = data[tail].val;
                    data[tail].val = sw;
                    example = CEB_push(example, args->unkBuffer);
                    inFlightTail[st.MPI_SOURCE] = (inFlightTail[st.MPI_SOURCE] + 1) % args->kParam;
                } else {
                    // hard, buffer will need compatcion
                    int found = 0;
                    for (size_t i = tail; i != head; i = ((i+1)%args->kParam)) {
                        if (found && i != tail) {
                            int prev = (i + args->kParam + 1) % args->kParam;
                            Example swa;
                            data[prev] = data[i];
                            data[prev].val = data[i].val;
                            sw = example->val;
                            example->val = data[i].val;
                        } else if (data[i].id == example->id) {
                            sw = example->val;
                            example->val = data[i].val;
                            data[i].val = sw;
                            example = CEB_push(example, args->unkBuffer);
                            found = 1;
                        }
                    }
                }
            }
            // while dest is root or dest has a full buffer
        } while (dest == MFOG_RANK_MAIN || ((inFlightHead[dest] + 1) % args->kParam) == inFlightTail[dest]);
        //
        assertMpi(MPI_Pack(example, sizeof(Example), MPI_BYTE, buffer, bufferSize, &position, MPI_COMM_WORLD));
        assertMpi(MPI_Pack(example->val, args->dim, MPI_DOUBLE, buffer, bufferSize, &position, MPI_COMM_WORLD));
        assertMpi(MPI_Send(buffer, position, MPI_PACKED, dest, MFOG_TAG_EXAMPLE, MPI_COMM_WORLD));
        //
        inFlight[dest][inFlightHead[dest]].id = example->id;
        sw = inFlight[dest][inFlightHead[dest]].val;
        inFlight[dest][inFlightHead[dest]].val = example->val;
        example->val = sw;
        inFlightHead[dest] = (inFlightHead[dest] + 1) % args->kParam;
        //
    }
    example->label = MFOG_EOS_MARKER;
    int position = 0;
    assertMpi(MPI_Pack(example, sizeof(Example), MPI_BYTE, buffer, bufferSize, &position, MPI_COMM_WORLD));
    assertMpi(MPI_Pack(example->val, args->dim, MPI_DOUBLE, buffer, bufferSize, &position, MPI_COMM_WORLD));
    for (dest = 1; dest < args->mpiSize; dest++) {
        assertMpi(MPI_Send(buffer, position, MPI_PACKED, dest, MFOG_TAG_EXAMPLE, MPI_COMM_WORLD));
    }
    fprintf(stderr, "[%s] %le seconds. At %s:%d\n", "sampler", ((double)clock() - start) / 1000000.0, __FILE__, __LINE__);
    return inputLine;
}

void *collector(void *arg) {
    clock_t start = clock();
    ThreadArgs *args = arg;
    size_t *inputLine = calloc(1, sizeof(size_t));
    int streams = args->mpiSize - 1;
    (*inputLine) = 0;
    Example *example = calloc(1, sizeof(Example));
    int unprocessedIdx = 0, unprocessedSize = 10000;
    Example **unprocessed = calloc(unprocessedSize, sizeof(Example *));
    for (int i = 0; i < unprocessedSize; i++) {
        unprocessed[i] = calloc(1, sizeof(Example));
        unprocessed[i]->id = -1;
        unprocessed[i]->label = '_';
        unprocessed[i]->val = calloc(args->dim, sizeof(double));
    }
    Example *sw;
    int mpiReturn;
    while (1) {
        assertMpi(MPI_Recv(example, sizeof(Example), MPI_BYTE, MPI_ANY_SOURCE, MFOG_TAG_UNKNOWN, MPI_COMM_WORLD, MPI_STATUS_IGNORE));
        printf("%10u,%s\n", example->id, printableLabel(example->label));
        if (example->label == UNK_LABEL) {
            printf("Unknown: %10u", example->id);
            for (unsigned int d = 0; d < args->dim; d++)
                printf(", %le", example->val[d]);
            printf("\n");
        }
        fflush(stdout);
    }
    fprintf(stderr, "[%s] %le seconds. At %s:%d\n", "detector", ((double)clock() - start) / 1000000.0, __FILE__, __LINE__);
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
    size_t *inputLine = calloc(1, sizeof(size_t));
    int streams = args->mpiSize - 1;
    (*inputLine) = 0;
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
    //
    // determina espaço necessário para empacotamento dos dados no transmissor e recebimento no nó tratador
    int clusterBufferLen, exampleBufferLen, valueBufferLen;
    int mpiReturn;
    assertMpi(MPI_Pack_size(sizeof(Cluster), MPI_BYTE, MPI_COMM_WORLD, &clusterBufferLen));
    assertMpi(MPI_Pack_size(sizeof(Example), MPI_BYTE, MPI_COMM_WORLD, &exampleBufferLen));
    assertMpi(MPI_Pack_size(args->dim, MPI_DOUBLE, MPI_COMM_WORLD, &valueBufferLen));
    int bufferSize = clusterBufferLen + exampleBufferLen + valueBufferLen + 2 * MPI_BSEND_OVERHEAD;
    int *buffer = (int *)malloc(bufferSize);
    //
    int unprocessedIdx = 0, unprocessedSize = 10000;
    Example **unprocessed = calloc(unprocessedSize, sizeof(Example *));
    for (int i = 0; i < unprocessedSize; i++) {
        unprocessed[i] = calloc(1, sizeof(Example));
        unprocessed[i]->id = -1;
        unprocessed[i]->label = '_';
        unprocessed[i]->val = calloc(dim, sizeof(double));
    }
    Example *sw;
    while (streams > 0) {
        assertMpi(MPI_Recv(buffer, bufferSize, MPI_PACKED, MPI_ANY_SOURCE, MFOG_TAG_UNKNOWN, MPI_COMM_WORLD, MPI_STATUS_IGNORE));
        int position = 0;
        // marker("MPI_Unpack");
        assertMpi(MPI_Unpack(buffer, bufferSize, &position, example, sizeof(Example), MPI_BYTE, MPI_COMM_WORLD));
        // if (example->id < 0) return 0;
        // assertMpi(MPI_Unpack(buffer, bufferSize, &position, valuePtr, args->dim, MPI_DOUBLE, MPI_COMM_WORLD));
        // example->val = valuePtr;
        // find the data value
        if (unprocessedIdx < 10) {
            ex = CEB_pop(ex, args->unkBuffer);
            if (ex->id == example->id) {
                example->val = ex->val;
            } else {
                int i;
                // look in unprocessed for example
                for (i = 0; i < unprocessedIdx; i++) {
                    if (unprocessed[i]->id == example->id) {
                        // found? swap
                        sw = unprocessed[i];
                        unprocessed[i] = ex;
                        ex = sw;
                        example->val = ex->val;
                        break;
                    }
                }
                // not found? add to queue and skip
                if (i == unprocessedIdx) {
                    sw = unprocessed[unprocessedIdx];
                    unprocessed[unprocessedIdx] = ex;
                    ex = sw;
                    unprocessedIdx++;
                    assert(unprocessedIdx < unprocessedSize);
                    continue;
                }
            }
        } else {
            // look in unprocessed for example
            for (int i = 0; i < unprocessedIdx; i++) {
                if (unprocessed[i]->id == example->id) {
                    // found? swap
                    example->val = unprocessed[i]->val;
                    sw = unprocessed[i];
                    unprocessed[i] = ex;
                    ex = sw;
                    break;
                }
            }
        }
        (*inputLine)++;
        if (example->label == MFOG_EOS_MARKER) {
            streams--;
            continue;
        }
        //
        id = example->id > id ? example->id : id;
        printf("%10u,%s\n", example->id, printableLabel(example->label));
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
            // marker("realloc unknowns");
            unknowns = realloc(unknowns, unknownsMaxSize * sizeof(Example));
        }
        //
        if (unknownsSize % noveltyDetectionTrigger == 0 && id - lastNDCheck > noveltyDetectionTrigger) {
            // marker("noveltyDetection");
            lastNDCheck = id;
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
            size_t reclassified = 0;
            for (size_t ex = 0; ex < unknownsSize; ex++) {
                // compress
                unknowns[ex - reclassified] = unknowns[ex];
                Cluster *nearest;
                double distance = nearestClusterVal(dim, &model->clusters[prevSize], nNewClusters, unknowns[ex].val, &nearest);
                assert(nearest != NULL);
                if (distance <= nearest->distanceMax) {
                    reclassified++;
                    free(unknowns[ex].val);
                }
            }
            fprintf(stderr, "Reclassified %lu\n", reclassified);
            unknownsSize -= reclassified;
        }
    }
    Cluster cl;
    cl.label = MFOG_EOS_MARKER;
    assertMpi(MPI_Bcast(&cl, sizeof(Cluster), MPI_BYTE, MFOG_RANK_MAIN, MPI_COMM_WORLD));
    fprintf(stderr, "[%s] %le seconds. At %s:%d\n", "detector", ((double)clock() - start) / 1000000.0, __FILE__, __LINE__);
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
    if (args.mpiRank == MFOG_RANK_MAIN)
        fprintf(stderr,
            "%s; kParam=%d; dim=%d; precision=%le; radiusF=%le; minExamplesPerCluster=%d; noveltyF=%le;\t"
            "Hello from %s, rank %d/%d\n",
            argv[0], PARAMS, args.mpiProcessorName, args.mpiRank, args.mpiSize);
    //
    assertErrno(sem_init(&args.modelReadySemaphore, 0, 0) >= 0, "Semaphore init fail%c.", '.', /**/);
    assertErrno(pthread_mutex_init(&args.modelMutex, NULL) >= 0, "Mutex init fail%c.", '.', /**/);
    //
    args.model = calloc(1, sizeof(Model));
    args.model->size = 0;
    args.model->nextLabel = '\0';
    args.model->clusters = calloc(kParam, sizeof(Cluster));
    //
    // args.unkBuffer = CEB_create(3, dim);
    // fprintf(stderr, "args.mpiRank %d\t", args.mpiRank);
    // for (int i = 0; i < args.unkBuffer->size; i++) {
    //     fprintf(stderr, "%p ", args.unkBuffer->data[i]->val);
    // }
    // fprintf(stderr, "\n");
    //
    int result;
    if (args.mpiRank == MFOG_RANK_MAIN) {
        printf("#pointId,label\n");
        fflush(stdout);
        args.unkBuffer = CEB_create(100, dim);

        pthread_t detector_t, sampler_t;
        int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg);
        assertErrno(pthread_create(&sampler_t, NULL, sampler, (void *)&args) == 0, "Thread creation fail%c.", '.', MPI_Finalize());
        assertErrno(pthread_create(&detector_t, NULL, detector, (void *)&args) == 0, "Thread creation fail%c.", '.', MPI_Finalize());
        //
        assertErrno(pthread_join(sampler_t, (void **)&result) == 0, "Thread join fail%c.", '.', MPI_Finalize());
        assertErrno(pthread_join(detector_t, (void **)&result) == 0, "Thread join fail%c.", '.', MPI_Finalize());
    } else {
        args.unkBuffer = CEB_create(3, dim);
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
