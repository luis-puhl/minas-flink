#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>

typedef struct {
    int id, value;
} Example;

typedef struct {
    Example **data;
    int size, head, tail;
    pthread_mutex_t headMutex, tailMutex;
} CircularExampleBuffer;

#define isBufferEmpty(buff) (buff->head == buff->tail)
#define isBufferFull(buff) ((buff->head + 1) % buff->size == buff->tail)

Example *push(Example *ex, CircularExampleBuffer *buff) {
    while (1) {
        pthread_mutex_lock(&buff->headMutex);
        pthread_mutex_lock(&buff->tailMutex);
        if (isBufferFull(buff)) {
            pthread_mutex_unlock(&buff->headMutex);
            pthread_mutex_unlock(&buff->tailMutex);
            continue;
        }
        pthread_mutex_unlock(&buff->tailMutex);
        int head = buff->head;
        buff->head = (buff->head + 1) % buff->size;
        pthread_mutex_unlock(&buff->headMutex);
        Example *swp = buff->data[head];
        buff->data[head] = ex;
        return swp;
    }
}
Example *pop(Example *ex, CircularExampleBuffer *buff) {
    while (1) {
        pthread_mutex_lock(&buff->headMutex);
        pthread_mutex_lock(&buff->tailMutex);
        if (isBufferEmpty(buff)) {
            pthread_mutex_unlock(&buff->headMutex);
            pthread_mutex_unlock(&buff->tailMutex);
            continue;
        }
        pthread_mutex_unlock(&buff->headMutex);
        int tail = buff->tail;
        buff->tail = (buff->tail + 1) % buff->size;
        pthread_mutex_unlock(&buff->tailMutex);
        Example *swp = buff->data[tail];
        buff->data[tail] = ex;
        return swp;
    }
}

void *producer(void *args) {
    CircularExampleBuffer *buff = args;
    Example *ex = calloc(1, sizeof(Example));
    int acc = 0;
    for (int i = 0; i < buff->size * 2; i++) {
        ex->id = i;
        acc += ex->id;
        ex = push(ex, buff);
    }
    printf("[pro] done, acc = %d\n", acc);
    return (void * )args;
}

void *consumer(void *args) {
    CircularExampleBuffer *buff = args;
    Example *ex = calloc(1, sizeof(Example));
    int acc = 0;
    for (int i = 0; i < buff->size * 2; i++) {
        ex = pop(ex, buff);
        if (ex == NULL) break;
        acc += ex->id;
    }
    printf("[con] done acc = %d\n", acc);
    return (void *)args;
}

int main(int argc, char const *argv[]) {
    int fail, nProducers, nConsumers, bufferSize;
    fail = argc != 4;
    if (!fail) {
        nProducers = atoi(argv[1]);
        nConsumers = atoi(argv[2]);
        bufferSize = atoi(argv[3]);
        fail = nProducers < 0 || nConsumers < 0 || bufferSize < 0;
    }
    if (fail) {
        printf("Circular Buffer producer-consumer\n");
        printf("Usage: %s <n-prod> <n-cons> <buff-size>\n", argv[0]);
        printf("eg: %s 2 2 2\n", argv[0]);
        return EXIT_FAILURE;
    }
    bufferSize = bufferSize < 2 ? 2 : bufferSize;
    printf("%s %d %d %d\n", argv[0], nProducers, nConsumers, bufferSize);
    //
    CircularExampleBuffer buff = {
        .head = 0, .tail = 0, .size = bufferSize,
    };
    buff.data = calloc(buff.size, sizeof(Example*));
    for (int i = 0; i < buff.size; i++) {
        buff.data[i] = calloc(1, sizeof(Example));
    }
    pthread_mutex_init(&buff.headMutex, NULL);
    pthread_mutex_init(&buff.tailMutex, NULL);
    //
    pthread_t *producer_t = calloc(nProducers, sizeof(pthread_t));
    pthread_t *consumer_t = calloc(nConsumers, sizeof(pthread_t));
    for (size_t i = 0; i < nProducers; i++)
        pthread_create(&producer_t[i], NULL, producer, (void *)&buff);
    for (size_t i = 0; i < nConsumers; i++)
        pthread_create(&consumer_t[i], NULL, consumer, (void *)&buff);
    for (size_t i = 0; i < nProducers; i++)
        pthread_join(producer_t[i], NULL);
    for (size_t i = 0; i < nConsumers; i++)
        pthread_join(consumer_t[i], NULL);
    //
    pthread_mutex_destroy(&buff.headMutex);
    pthread_mutex_destroy(&buff.tailMutex);
    return 0;
}
