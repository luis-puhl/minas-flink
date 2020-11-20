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

Example *CEB_push(Example *ex, CircularExampleBuffer *buff) {
    while (1) {
        pthread_mutex_lock(&buff->headMutex);
        pthread_mutex_lock(&buff->tailMutex);
        int isBufferFull = (buff->head + 1) % buff->size == buff->tail;
        if (isBufferFull) {
            pthread_mutex_unlock(&buff->headMutex);
            pthread_mutex_unlock(&buff->tailMutex);
            sem_wait(&buff->notFullSignal);
            // marker("push @ full buffer");
            // pthread_mutex_lock(&buff->tailMutex);
            // pthread_cond_wait(&buff->notFullSignal, &buff->tailMutex);
            // pthread_mutex_unlock(&buff->tailMutex);
            continue;
        }
        pthread_mutex_unlock(&buff->tailMutex);
        int head = buff->head;
        buff->head = (buff->head + 1) % buff->size;
        pthread_mutex_unlock(&buff->headMutex);
        sem_post(&buff->notEmptySignal);
        Example *swp = buff->data[head];
        buff->data[head] = ex;
        return swp;
    }
}
Example *CEB_pop(Example *ex, CircularExampleBuffer *buff) {
    while (1) {
        pthread_mutex_lock(&buff->headMutex);
        pthread_mutex_lock(&buff->tailMutex);
        int isBufferEmpty = buff->head == buff->tail;
        if (isBufferEmpty) {
            pthread_mutex_unlock(&buff->headMutex);
            pthread_mutex_unlock(&buff->tailMutex);
            // marker("pop @ empty buffer");
            sem_wait(&buff->notEmptySignal);
            // pthread_mutex_lock(&buff->headMutex);
            // pthread_cond_wait(&buff->notEmptySignal, &buff->headMutex);
            // pthread_mutex_unlock(&buff->headMutex);
            continue;
        }
        pthread_mutex_unlock(&buff->headMutex);
        int tail = buff->tail;
        buff->tail = (buff->tail + 1) % buff->size;
        pthread_mutex_unlock(&buff->tailMutex);
        sem_post(&buff->notFullSignal);
        Example *swp = buff->data[tail];
        buff->data[tail] = ex;
        return swp;
    }
}

CircularExampleBuffer *CEB_create(int bufferSize, int dim) {
    CircularExampleBuffer *buff = calloc(1, sizeof(CircularExampleBuffer));
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
