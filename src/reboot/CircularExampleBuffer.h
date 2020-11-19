#ifndef _CEB_H
#define _CEB_H 1
#include <pthread.h>

#include "../reboot/base.h"

typedef struct {
    Example **data;
    int size, head, tail;
    pthread_mutex_t headMutex, tailMutex;
    pthread_cond_t notFullSignal, notEmptySignal;
} CircularExampleBuffer;

Example *CEB_push(Example *ex, CircularExampleBuffer *buff);
Example *CEB_pop(Example *ex, CircularExampleBuffer *buff);

CircularExampleBuffer *CEB_create(int kParam, int dim);
void CEB_destroy(CircularExampleBuffer *buff);

#endif // _CEB_H
