#ifndef _CEB_C
#define _CEB_C 1

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <semaphore.h>

#include "../reboot/base.h"
#include "../reboot/CircularExampleBuffer.h"

Example *CEB_enqueue(CircularExampleBuffer *buff, Example *ex) {
    while (1) {
        pthread_mutex_lock(&buff->headMutex);
        pthread_mutex_lock(&buff->tailMutex);
        if (isBufferFull(buff)) {
            pthread_mutex_unlock(&buff->headMutex);
            pthread_mutex_unlock(&buff->tailMutex);
            sem_wait(&buff->notFullSignal);
            continue;
        }
        pthread_mutex_unlock(&buff->tailMutex);
        int head = buff->head;
        buff->head = (buff->head + 1) % buff->size;
        sem_post(&buff->notEmptySignal);
        Example *swp = buff->data[head];
        buff->data[head] = ex;
        pthread_mutex_unlock(&buff->headMutex);
        return swp;
    }
}
Example *CEB_dequeue(CircularExampleBuffer *buff, Example *ex) {
    while (1) {
        pthread_mutex_lock(&buff->headMutex);
        pthread_mutex_lock(&buff->tailMutex);
        if (isBufferEmpty(buff)) {
            pthread_mutex_unlock(&buff->headMutex);
            pthread_mutex_unlock(&buff->tailMutex);
            sem_wait(&buff->notEmptySignal);
            continue;
        }
        pthread_mutex_unlock(&buff->headMutex);
        int tail = buff->tail;
        buff->tail = (buff->tail + 1) % buff->size;
        sem_post(&buff->notFullSignal);
        Example *swp = buff->data[tail];
        buff->data[tail] = ex;
        pthread_mutex_unlock(&buff->tailMutex);
        return swp;
    }
}
Example *CEB_extract(CircularExampleBuffer *buff, Example *ex) {
    while (1) {
        pthread_mutex_lock(&buff->headMutex);
        pthread_mutex_lock(&buff->tailMutex);
        if (isBufferEmpty(buff)) {
            pthread_mutex_unlock(&buff->headMutex);
            pthread_mutex_unlock(&buff->tailMutex);
            sem_wait(&buff->notEmptySignal);
            continue;
        }
        pthread_mutex_unlock(&buff->headMutex);
        // if (len == 0)
        int tail = buff->tail;
        int head = buff->head;
        //     fprintf(stderr, "buff shouldn't be empty but got i=%d sending me ex=%d with buffer size=%d\n", st.MPI_SOURCE, example->id, len);
        if (buff->data[tail]->id != ex->id) {
            for (size_t i = (tail + 1) % buff->size; i != ((head+buff->size+1)%buff->size); i = ((i+1)%buff->size)) {
                if (buff->data[i]->id == ex->id) {
                    // swap tail and i
                    Example *e = buff->data[i];
                    buff->data[i] = buff->data[tail];
                    buff->data[tail] = e;
                    break;
                }
            }
        }
        buff->tail = (buff->tail + 1) % buff->size;
        pthread_mutex_unlock(&buff->tailMutex);
        sem_post(&buff->notFullSignal);
        Example *swp = buff->data[tail];
        buff->data[tail] = ex;
        return swp;
    }
}

CircularExampleBuffer *CEB_init(CircularExampleBuffer *buff, int bufferSize, int dim) {
    buff->head = 0;
    buff->tail = 0;
    buff->size = bufferSize;
    buff->data = calloc(buff->size, sizeof(Example*));
    for (int i = 0; i < buff->size; i++) {
        buff->data[i] = calloc(1, sizeof(Example));
        buff->data[i]->id = -1;
        buff->data[i]->label = '_';
        buff->data[i]->val = calloc(dim, sizeof(double));
    }
    assertErrno(pthread_mutex_init(&buff->headMutex, NULL) == 0, "Mutex init fail%c.", '.', /**/);
    assertErrno(pthread_mutex_init(&buff->tailMutex, NULL) == 0, "Mutex init fail%c.", '.', /**/);
    assertErrno(sem_init(&buff->notFullSignal, 0, 0) == 0, "Condition signal init fail%c.", '.', /**/);
    assertErrno(sem_init(&buff->notEmptySignal, 0, 0) == 0, "Condition signal init fail%c.", '.', /**/);
    return buff;
}

void CEB_destroy(CircularExampleBuffer *buff) {
    pthread_mutex_destroy(&buff->headMutex);
    pthread_mutex_destroy(&buff->tailMutex);
    sem_destroy(&buff->notFullSignal);
    sem_destroy(&buff->notEmptySignal);
    for (int i = 0; i < buff->size; i++) {
        free(buff->data[i]->val);
        free(buff->data[i]);
    }
    free(buff->data);
    free(buff);
}

#endif // _CEB_C
