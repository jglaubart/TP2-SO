#ifndef TP_SO_MEMORYMANAGER_H
#define TP_SO_MEMORYMANAGER_H

#include <stddef.h>

void initMemory(void);

void * myMalloc(size_t size);

void myFree(void *ptr);

void memstats(size_t *total, size_t *used, size_t *available);

int isValidHeapPtr(void *ptr);

#endif //TP_SO_MEMORYMANAGER_H