#ifndef _QUEUE_H
#define _QUEUE_H

#include <stddef.h>
#include <stdint.h>

#include "lib.h"  // Para memcpy

typedef struct QueueCDT * QueueADT; // queue con roundrobin
typedef int (*QueueElemCmpFn)(void *, void *);

QueueADT createQueue(QueueElemCmpFn cmp, int elemSize);
QueueADT enqueue(QueueADT queue, void * data);
void * dequeue(QueueADT queue, void * buffer);
void * queuePeek(QueueADT queue, void * buffer);
void * queueRemove(QueueADT queue, void * data);
void queueFree(QueueADT queue);
int queueSize(QueueADT queue);
QueueADT queueBeginCyclicIter(QueueADT queue);
void * queueNextCyclicIter(QueueADT queue, void * buffer);
int queueCyclicIterLooped(QueueADT queue);
int queueIsEmpty(QueueADT queue);
int queueIteratorIsInitialized(QueueADT queue);
int queueElementExists(QueueADT queue, void * data);

#endif