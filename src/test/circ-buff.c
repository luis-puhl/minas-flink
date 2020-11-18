#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>

typedef struct {
    int id, value;
} Example;

typedef struct {
    Example **buff;
    int size, head, tail, len, activeProducers;
    pthread_mutex_t mutex;
    pthread_cond_t consumerSignal;
    pthread_cond_t producerSignal;
} CircularExampleBuffer;

Example *push(Example *ex, CircularExampleBuffer *buff) {
    Example *swp;
    // pthread_mutex_lock(&buff->mutex);
    while (buff->len == buff->size) {
        // pthread_cond_signal(&buff->consumerSignal);
        // cond_wait unlocks mutex, blocks thread, resumes on signal, locks mutex
        // pthread_cond_wait(&buff->producerSignal, &buff->mutex);
    }
    swp = buff->buff[buff->head];
    buff->buff[buff->head] = ex;
    buff->len++;
    buff->head = (buff->head + 1) % buff->size;
    if (buff->len == 1) {
        // pthread_cond_signal(&buff->consumerSignal);
    }
    // pthread_mutex_unlock(&buff->mutex);
    return swp;
}
Example *pop(Example *ex, CircularExampleBuffer *buff) {
    Example *swp;
    // pthread_mutex_lock(&buff->mutex);
    while (buff->len == 0) {
        printf("[pop] activeProducers %d\n", buff->activeProducers);
        if (!buff->activeProducers) {
            // pthread_mutex_unlock(&buff->mutex);
            return NULL;
        }
        // cond_wait unlocks mutex, blocks thread, resumes on signal, locks mutex
        // pthread_cond_signal(&buff->producerSignal);
        // pthread_cond_wait(&buff->consumerSignal, &buff->mutex);
    }
    swp = buff->buff[buff->tail];
    buff->buff[buff->tail] = ex;
    buff->len--;
    buff->tail = (buff->tail + 1) % buff->size;
    if (buff->len == (buff->size -1)) {
        // pthread_cond_signal(&buff->producerSignal);
    }
    // pthread_mutex_unlock(&buff->mutex);
    return swp;
}

void *producer(void *args) {
    pthread_t tid = pthread_self();
    CircularExampleBuffer *buff = args;
    // pthread_mutex_lock(&buff->mutex);
    buff->activeProducers++;
    printf("[pro %lu] activeProducers %d\n", tid, buff->activeProducers);
    // pthread_cond_broadcast(&buff->consumerSignal);
    // pthread_mutex_unlock(&buff->mutex);
    Example *ex = calloc(1, sizeof(Example));
    int acc = 0;
    for (int i = 0; i < buff->size * 2; i++) {
        ex->id = i;
        // printf("[pro %lu] %d\n", tid, ex->id);
        acc += ex->id;
        ex = push(ex, buff);
    }
    // pthread_mutex_lock(&buff->mutex);
    buff->activeProducers--;
    printf("[pro %lu] done activeProducers %d, acc = %d\n", tid, buff->activeProducers, acc);
    // pthread_mutex_unlock(&buff->mutex);
    return (void * )args;
}

void *consumer(void *args) {
    pthread_t tid = pthread_self();
    CircularExampleBuffer *buff = args;
    // printf("[con] buff %d, %p\n", buff->size, buff->buff);
    Example *ex = calloc(1, sizeof(Example));
    // pthread_mutex_lock(&buff->mutex);
    while(!buff->activeProducers) {
        // pthread_cond_wait(&buff->consumerSignal, &buff->mutex);
    }
    printf("[con %lu] activeProducers %d\n", tid, buff->activeProducers);
    // pthread_mutex_unlock(&buff->mutex);
    int acc = 0;
    for (int i = 0; i < buff->size * 2; i++) {
        ex = pop(ex, buff);
        if (ex == NULL) break;
        acc += ex->id;
        // printf("[con %lu] %d\n", tid, ex->id);
    }
    printf("[con %lu] done acc = %d\n", tid, acc);
    return (void *)args;
}

int main(int argc, char const *argv[]) {
    if (argc != 4) {
        printf("Circular Buffer producer-consumer\n");
        printf("Usage: %s <n-prod> <n-cons> <buff-size>\n", argv[0]);
        printf("eg: %s 2 2 2\n", argv[0]);
        return EXIT_FAILURE;
    }
    int nProducers = atoi(argv[1]);
    int nConsumers = atoi(argv[2]);
    int bufferSize = atoi(argv[3]);
    printf("%s %d %d %d\n", argv[0], nProducers, nConsumers, bufferSize);
    //
    CircularExampleBuffer buff = {
        .head = 0,
        .tail = 0,
        .len = 0,
        .activeProducers = 0,
        .size = bufferSize,
    };
    buff.buff = calloc(buff.size, sizeof(Example*));
    for (int i = 0; i < buff.size; i++) {
        buff.buff[i] = calloc(1, sizeof(Example));
    }
    pthread_cond_init(&buff.consumerSignal, NULL);
    pthread_cond_init(&buff.producerSignal, NULL);
    pthread_mutex_init(&buff.mutex, NULL);
    //
    pthread_t *producer_t = calloc(nProducers, sizeof(pthread_t));
    pthread_t *consumer_t = calloc(nConsumers, sizeof(pthread_t));
    for (size_t i = 0; i < nProducers; i++) {
        pthread_create(&producer_t[i], NULL, producer, (void *)&buff);
    }
    for (size_t i = 0; i < nConsumers; i++) {
        pthread_create(&consumer_t[i], NULL, consumer, (void *)&buff);
    }
    for (size_t i = 0; i < nProducers; i++) {
        pthread_join(producer_t[i], NULL);
    }
    for (size_t i = 0; i < nConsumers; i++) {
        pthread_join(consumer_t[i], NULL);
    }
    //
    pthread_cond_destroy(&buff.consumerSignal);
    pthread_cond_destroy(&buff.producerSignal);
    pthread_mutex_destroy(&buff.mutex);
    return 0;
}
