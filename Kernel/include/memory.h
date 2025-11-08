#ifndef TP_SO_MEMORYMANAGER_H
#define TP_SO_MEMORYMANAGER_H

#include <stddef.h>

// Initializes the memory manager
void initMemory(void);

// Reserves memory
void * myMalloc(int size);

// Frees memory
void myFree(void *ptr);

// Gets memory statistics
void memstats(int *total, int *used, int *available);

// Validates if a pointer is within the heap and aligned
int isValidHeapPtr(void *ptr);

#endif //TP_SO_MEMORYMANAGER_H