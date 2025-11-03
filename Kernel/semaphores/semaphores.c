#include "semaphores.h"
#include "lib.h"
#include "memory.h"
#include "process.h"
#include "scheduler.h"
#include "panic.h"
#include "queue.h"
#include "strings.h"

struct semCDT {
    char * name;
    uint32_t count;
    uint8_t lock;
    QueueADT blocked_processes;
};

static QueueADT semaphoreQueue = NULL;
uint8_t queueLock = 0;

int cmpSem(void * sem_a, void * sem_b) {
    return *((semADT *)sem_a) - *((semADT *)sem_b);
}

static int cmpInt(void *a, void *b) {
    return *((int *)a) - *((int *)b);
}

int initSemaphoreQueue() {
    semaphoreQueue = createQueue(cmpSem, sizeof(semADT));
    if (semaphoreQueue == NULL) {
        return -1;
    }
    return 0;
}

void semInit(semADT sem, const char *name, uint32_t initial_count){
    if(sem == NULL || name == NULL){
        return;
    }
    sem->name = myMalloc(strlen(name) + 1);
    if (sem->name == NULL) {
        return;
    }
    strcpy(sem->name, name);
    sem->count = initial_count;
    sem->lock = 0;
    sem->blocked_processes = createQueue(cmpInt, sizeof(int));

    semLock(&queueLock);
    if (!queueElementExists(semaphoreQueue, &sem)) {
        enqueue(semaphoreQueue, &sem);
    }
    semUnlock(&queueLock);
}

int post(semADT sem){}
int wait(semADT sem){}
void semDestroy(semADT sem){}
