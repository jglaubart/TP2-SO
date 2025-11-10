// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

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
    semADT a = sem_a == NULL ? NULL : *((semADT *)sem_a);
    semADT b = sem_b == NULL ? NULL : *((semADT *)sem_b);

    if (a == b) {
        return 0;
    }

    if (a == NULL || b == NULL) {
        return 1;
    }

    if (a->name == NULL || b->name == NULL) {
        return (a->name != b->name);
    }

    return strcmp(a->name, b->name);
}

static int cmpInt(void *a, void *b) {
    return *((int *)a) - *((int *)b);
}

static semADT findSemaphoreByName(const char *name) {
    if (name == NULL || semaphoreQueue == NULL || queueIsEmpty(semaphoreQueue)) {
        return NULL;
    }

    if (queueBeginCyclicIter(semaphoreQueue) == NULL) {
        return NULL;
    }

    semADT current = NULL;
    int size = queueSize(semaphoreQueue);

    for (int i = 0; i < size; i++) {
        queueNextCyclicIter(semaphoreQueue, &current);
        if (strcmp(current->name, name) == 0) {
            return current;
        }
    }

    return NULL;
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

    if (semaphoreQueue == NULL) {
        QueueADT newQueue = createQueue(cmpSem, sizeof(semADT));
        if (newQueue == NULL) {
            return NULL;
        }

        semLock(&queueLock);
        if (semaphoreQueue == NULL) {
            semaphoreQueue = newQueue;
            newQueue = NULL;
        }
        semUnlock(&queueLock);

        if (newQueue != NULL) {
            queueFree(newQueue);
        }
    }

    semLock(&queueLock);
    semADT existing = findSemaphoreByName(name);
    if (existing != NULL) {
        semUnlock(&queueLock);
        return existing;
    }
    semUnlock(&queueLock);

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
    existing = findSemaphoreByName(name);
    if (existing != NULL) {
        semUnlock(&queueLock);
        queueFree(sem->blocked_processes);
        myFree(sem->name);
        myFree(sem);
        return existing;
    }

    if (enqueue(semaphoreQueue, &sem) == NULL) {
        semUnlock(&queueLock);
        queueFree(sem->blocked_processes);
        myFree(sem->name);
        myFree(sem);
        return NULL;
    }
    semUnlock(&queueLock);
    return sem;
}

int post(semADT sem){
    if (sem == NULL) {
        return -1;
    }

    semLock(&sem->lock);
    if(queueIsEmpty(sem->blocked_processes)) {
        sem->count++;
        semUnlock(&sem->lock);
    } else {
        int pid;
        dequeue(sem->blocked_processes, &pid);
        semUnlock(&sem->lock);
        unblock(pid);
        // Yield to give the newly unblocked process a chance to run immediately
        // This is especially important for semaphores where the unblocked process
        // is likely waiting for the same resource we just released
        yield();
    }
    return 0;
}

int wait(semADT sem){
    if (sem == NULL) {
        return -1;
    }

    semLock(&sem->lock);
    if (sem->count > 0) {
        sem->count--;
        semUnlock(&sem->lock);
    } else {
        Process * currentProcess = getCurrentProcess();
        int pid = currentProcess->pid;
        enqueue(sem->blocked_processes, &pid);
        semUnlock(&sem->lock);
        block(pid);
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

int semGetBlockedCount(semADT sem) {
    if (sem == NULL) {
        return -1;
    }
    semLock(&sem->lock);
    int count = queueSize(sem->blocked_processes);
    semUnlock(&sem->lock);
    return count;
}

void wakeBlocked(semADT sem) {
    if (sem == NULL) {
        return;
    }

    int blocked = semGetBlockedCount(sem);
    while (blocked-- > 0) {
        post(sem);
    }
}