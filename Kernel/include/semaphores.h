#ifndef SEMAPHORES_H
#define SEMAPHORES_H

#include <stdint.h>
#include <queue.h>

typedef struct semaphore {
    char *name;
    uint32_t count;
    uint8_t lock;
    QueueADT *waiting_processes;
} Semaphore;

void sem_init(Semaphore *sem, const char *name, uint32_t initial_count);
int sem_post(Semaphore *sem);
int sem_wait(Semaphore *sem);
void sem_destroy(Semaphore *sem);

#endif