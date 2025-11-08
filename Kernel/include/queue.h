#ifndef _QUEUE_H
#define _QUEUE_H

#include <stddef.h>
#include <stdint.h>

#include "lib.h" 

typedef struct QueueCDT * QueueADT;
typedef int (*QueueElemCmpFn)(void *, void *);

// Creates a queue, uses cmp for comparisons.
QueueADT createQueue(QueueElemCmpFn cmp, int elemSize);
QueueADT enqueue(QueueADT queue, void * data);
// Removes the head element and copies it into buffer.
void * dequeue(QueueADT queue, void * buffer);
// Copies the head element into buffer without removing it.
void * queuePeek(QueueADT queue, void * buffer);

// Removes the first element that matches data (using cmp).
void * queueRemove(QueueADT queue, void * data);
void queueFree(QueueADT queue);
int queueSize(QueueADT queue);
int queueIsEmpty(QueueADT queue);
int queueElementExists(QueueADT queue, void * data);

//======================= CYCLIC ITERATOR ==================

// Starts a round-robin iteration over the queue.
QueueADT queueBeginCyclicIter(QueueADT queue);
// Retrieves the next element in the round-robin iteration into buffer.
void * queueNextCyclicIter(QueueADT queue, void * buffer);

// Indicates whether the round-robin iteration completed a full loop.
int queueCyclicIterLooped(QueueADT queue);

// Reports if the round-robin iterator has been initialized.
int queueIteratorIsInitialized(QueueADT queue);

#endif
