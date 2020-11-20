#ifndef _CEB_H
#define _CEB_H 1
#include <pthread.h>
#include <semaphore.h>

#include "../reboot/base.h"

typedef struct {
    Example **data;
    int size, head, tail;
    pthread_mutex_t headMutex, tailMutex;
    sem_t notFullSignal, notEmptySignal;
} CircularExampleBuffer;

#define isBufferFull(buff) (buff->head + 1) % buff->size == buff->tail
#define isBufferEmpty(buff) buff->head == buff->tail

CircularExampleBuffer *CEB_init(CircularExampleBuffer *buff, int kParam, int dim);
void CEB_destroy(CircularExampleBuffer *buff);

Example *CEB_enqueue(CircularExampleBuffer *buff, Example *ex);
Example *CEB_dequeue(CircularExampleBuffer *buff, Example *ex);

Example *CEB_extract(CircularExampleBuffer *buff, Example *ex);

#endif // _CEB_H
