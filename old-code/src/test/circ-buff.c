#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>

#include "../reboot/base.h"
#include "../reboot/CircularExampleBuffer.h"

void *producer(void *args) {
    CircularExampleBuffer *buff = args;
    Example *ex = calloc(1, sizeof(Example));
    int acc = 0;
    for (int i = 0; i < buff->size * 2; i++) {
        ex->id = i;
        ex->label = (i % 27) + 'a';
        if (ex->label < 'a' || ex->label > 'z')
            errx(EXIT_FAILURE, "wowowwow prod %c", ex->label);
        fprintf(stderr, "%c", ex->label);
        acc += ex->id;
        ex = CEB_enqueue(buff, ex);
    }
    free(ex);
    fprintf(stderr, "\n");
    printf("[pro] done acc = %d\n", acc);
    return (void * )args;
}

void *consumer(void *args) {
    CircularExampleBuffer *buff = args;
    Example *ex = calloc(1, sizeof(Example));
    ex->id = 0;
    ex->label = '_';
    int acc = 0;
    for (int i = 0; i < buff->size * 2; i++) {
        ex = CEB_dequeue(buff, ex);
        if (ex == NULL)
            errx(EXIT_FAILURE, "\nwowowwow cons NULL\n");
        fprintf(stderr, "%c", ex->label - ' ');
        if (ex->label == '-') break;
        if (ex->label < 'a' || ex->label > 'z')
            errx(EXIT_FAILURE, "\nwowowwow cons '%c' '#%d' %d\n", ex->label, ex->label, ex->id);
        acc += ex->id;
    }
    free(ex);
    fprintf(stderr, "\n");
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
    CircularExampleBuffer *buff = CEB_init(calloc(1, sizeof(CircularExampleBuffer)), bufferSize, 2);
    //
    pthread_t *producer_t = calloc(nProducers, sizeof(pthread_t));
    pthread_t *consumer_t = calloc(nConsumers, sizeof(pthread_t));
    for (size_t i = 0; i < nProducers; i++)
        pthread_create(&producer_t[i], NULL, producer, (void *)buff);
    for (size_t i = 0; i < nConsumers; i++)
        pthread_create(&consumer_t[i], NULL, consumer, (void *)buff);
    for (size_t i = 0; i < nProducers; i++)
        pthread_join(producer_t[i], NULL);
    Example *ex = calloc(1, sizeof(Example));
    for (size_t i = 0; i < nConsumers && i < (bufferSize -1); i++) {
        ex->id = i;
        ex->label = '-';
        ex = CEB_enqueue(buff, ex);
    }
    free(ex);
    for (size_t i = 0; i < nConsumers; i++)
        pthread_join(consumer_t[i], NULL);
    //
    CEB_destroy(buff);
    return 0;
}
