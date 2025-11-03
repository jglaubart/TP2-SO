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

// queueLock acts as a mutex for critical regions
static QueueADT semaphoreQueue = NULL;
uint8_t queueLock = 0;

int cmpSem(void * sem_a, void * sem_b) {
    return *((semADT *)sem_a) - *((semADT *)sem_b);
}

static int cmpInt(void *a, void *b) {
    return *((int *)a) - *((int *)b);
}

int initSemaphoreQueue(void) {
    semaphoreQueue = createQueue(cmpSem, sizeof(semADT));
    if (semaphoreQueue == NULL) {
        return -1;
    }
    return 0;
}

semADT semInit(const char *name, uint32_t initial_count){
    if (name == NULL) {
        return NULL;
    }

    semADT sem = myMalloc(sizeof(struct semCDT));
    if (sem == NULL) {
        return NULL;
    }

    sem->name = myMalloc(strlen(name) + 1);
    if (sem->name == NULL) {
        myFree(sem);
        return NULL;
    }
    strcpy(sem->name, name);
    sem->count = initial_count;
    sem->lock = 0;
    sem->blocked_processes = createQueue(cmpInt, sizeof(int));
    if( sem->blocked_processes == NULL) {
        myFree(sem->name);
        myFree(sem);
        return NULL;
    }

    semLock(&queueLock);
    if (!queueElementExists(semaphoreQueue, &sem)) {
        enqueue(semaphoreQueue, &sem);
    }
    semUnlock(&queueLock);
    return sem;
}

int post(semADT sem){
    if (sem == NULL) {
        return -1;
    }

    semLock(&queueLock);
    if(queueIsEmpty(sem->blocked_processes)) {
        sem->count++;
    } else {
        int pid;
        dequeue(sem->blocked_processes, &pid);
        unblock(pid);
    }
    semUnlock(&queueLock);
    return 0;
}

int wait(semADT sem){
    if (sem == NULL) {
        return -1;
    }

    semLock(&queueLock);
    if (sem->count > 0) {
        sem->count--;
        semUnlock(&queueLock);
    } else {
        Process * currentProcess = getCurrentProcess();
        int pid = currentProcess->pid;
        enqueue(sem->blocked_processes, &pid);
        semUnlock(&queueLock);
        block(pid);
        yield();
    }
    return 0;
}

void semDestroy(semADT sem){
    if (sem == NULL) {
        return;
    }

    semLock(&queueLock);
    queueRemove(semaphoreQueue, &sem);
    semUnlock(&queueLock);

    // Unblock all processes waiting on this semaphore
    while (!queueIsEmpty(sem->blocked_processes)) {
        int pid;
        dequeue(sem->blocked_processes, &pid);
        unblock(pid); 
    }
    queueFree(sem->blocked_processes);
    myFree(sem->name);
    myFree(sem);
}
