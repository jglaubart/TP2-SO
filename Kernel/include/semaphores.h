#ifndef SEMAPHORES_H
#define SEMAPHORES_H

#include <stdint.h>
#include <queue.h>

typedef struct semCDT * semADT;

semADT semInit(const char *name, uint32_t initial_count);
int post(semADT sem);
int wait(semADT sem);
void semDestroy(semADT sem);
int semGetBlockedCount(semADT sem);

void semLock(uint8_t *lock);
void semUnlock(uint8_t *lock);

int initSemaphoreQueue(void);

#endif