#ifndef SEMAPHORES_H
#define SEMAPHORES_H

#include <stdint.h>
#include <queue.h>

typedef struct semCDT * semADT;

void semInit(semADT sem, const char *name, uint32_t initial_count);
int post(semADT sem);
int wait(semADT sem);
void semDestroy(semADT sem);

#endif