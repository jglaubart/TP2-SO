#ifndef TP_SO_MEMORYMANAGER_H
#define TP_SO_MEMORYMANAGER_H

#include <stddef.h>

void initMemory(void);

void * myMalloc(int size);

void myFree(void *ptr);

void memstats(int *total, int *used, int *available);

int isValidHeapPtr(void *ptr);

#endif //TP_SO_MEMORYMANAGER_H