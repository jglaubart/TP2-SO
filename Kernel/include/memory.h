#ifndef TP_SO_MEMORYMANAGER_H
#define TP_SO_MEMORYMANAGER_H

#include <stddef.h>

void init(void);

void *malloc(size_t size);

void free(void *ptr);

void memstats(size_t *total, size_t *used, size_t *available);

#endif //TP_SO_MEMORYMANAGER_H