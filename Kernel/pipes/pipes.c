#include "pipes.h"
#include <stdint.h>
#include <stddef.h>
#include "memory.h"
#include "panic.h"
#include "process.h"
#include "scheduler.h"
#include "lib.h"
#include "semaphores.h"
#include "strings.h"

#define FALSE 0
#define TRUE !FALSE
#define NEXT_IDX(i) (((i) + 1) % PIPE_BUFFER_SIZE)

struct pipeCDT {
    int id;
    int readIndex;
    int writeIndex;
    uint8_t buffer[PIPE_BUFFER_SIZE];
    semADT readSem;
    semADT writeSem;
    int refCount;
    int closed;
    uint8_t lock;
};

static pipeADT * pipes = NULL;
static int pipe_count = 0;

// used a static buffer to avoid dynamic allocation in getSemName
static char semNameBuffer[16];
static char * getSemName(char mode) {
    semNameBuffer[4] = '0' + (pipe_count / 10);
    semNameBuffer[5] = '0' + (pipe_count % 10);
    semNameBuffer[6] = mode;
    return semNameBuffer;
}

void initPipes() {
    pipes = myMalloc(sizeof(pipeADT) * MAX_PIPES);
    if(pipes == NULL){
        panic("Failed to allocate memory for pipes");
    }

    pipe_count = 0;
    strcpy(semNameBuffer, "pipeXXY");
    semNameBuffer[7] = '\0';
}

static pipeADT buildPipe() {
    if(pipe_count >= MAX_PIPES){
        return NULL;
    }

    pipeADT newPipe = myMalloc(sizeof(struct pipeCDT));
    if(newPipe == NULL){
        return NULL;
    }

    newPipe->id = pipe_count;
    newPipe->readIndex = 0;
    newPipe->writeIndex = 0;
    newPipe->refCount = 1;
    newPipe->closed = 0;
    newPipe->lock = 0;

    // Initialize semaphores
    getSemName('R');
    newPipe->readSem = semInit(semNameBuffer, 0);
    if(newPipe->readSem == NULL){
        myFree(newPipe);
        return NULL;
    }
    getSemName('W');
    newPipe->writeSem = semInit(semNameBuffer, PIPE_BUFFER_SIZE);
    if(newPipe->writeSem == NULL){
        semDestroy(newPipe->readSem);
        newPipe->readSem = NULL;
        myFree(newPipe);
        return NULL;
    }
    return newPipe;
}

int openPipe() {
    if(pipe_count >= MAX_PIPES){
        return -1;
    }

    pipeADT pipe = buildPipe();
    if(pipe == NULL){
        return -1;
    }
    pipes[pipe_count] = pipe;
    pipe_count++;
    return pipe->id;
}

static pipeADT getPipe(int pipeID) {
    if (pipeID < 0 || pipeID >= pipe_count) {
        return NULL;
    }
    return pipes[pipeID];
}

static void freePipe(pipeADT pipe) {
    if (pipe == NULL) {
        return;
    }
    semDestroy(pipe->readSem);
    semDestroy(pipe->writeSem);
    myFree(pipe);
}


static void tryFinalizePipe(int pipeID) {
    pipeADT pipe = getPipe(pipeID);
    if (pipe == NULL) {
        return;
    }
    if (!pipe->closed || pipe->refCount > 0) {
    return;
    }

    pipes[pipe->id] = NULL;
    freePipe(pipe);
}

int closePipe(int pipeID) {
    pipeADT pipe = getPipe(pipeID);
    if (pipe == NULL) {
        return -1;
    }

    if (pipe->refCount > 0) {
        pipe->refCount--;
    }

    // If there are still references, just return
    if (pipe->refCount > 0) {
        return 0;
    }

    // Otherwise we destroy the pipe
    pipe->closed = TRUE;

    wakeBlocked(pipe->readSem);
    wakeBlocked(pipe->writeSem);

    destroyPipe(pipe);
    return 0;
}

static int pipeHasData(pipeADT pipe) {
    return pipe->readIndex != pipe->writeIndex;
}

int readPipe(int pipeID, uint8_t * buffer, int size) {
    pipeADT pipe = getPipe(pipeID);
    if (pipe == NULL || buffer == NULL || size < 0) {
        return -1;
    }

    if (size == 0) {
        return 0;
    }

    int bytesRead = 0;

    while (bytesRead < size) {
        if (pipe->closed && !pipeHasData(pipe)) {
            break;
        }

        if (wait(pipe->readSem) != 0) {
            break;
        }

        semLock(&pipe->lock);

        if (!pipeHasData(pipe)) {
            semUnlock(&pipe->lock);
            if (pipe->closed) {
                break;
            }
            continue;
        }

        buffer[bytesRead] = pipe->buffer[pipe->readIndex];
        pipe->readIndex = NEXT_IDX(pipe->readIndex);
        semUnlock(&pipe->lock);

        if (post(pipe->writeSem) != 0) {
            panic("Pipe write semaphore failed");
        }

        bytesRead++;
    }

    tryFinalizePipe(pipeID);
    return bytesRead;
}

int writePipe(int pipeID, uint8_t * buffer, int size) {
    pipeADT pipe = getPipe(pipeID);
    if (pipe == NULL || buffer == NULL || size < 0)
        return -1;
    if (pipe->closed)
        return -1;
    if (size == 0) 
        return 0;

    int written = 0;

    while (written < size) {
        if (pipe->closed) {
            break;
        }

        if (wait(pipe->writeSem) != 0) {
            break;
        }

        if (pipe->closed) {
            post(pipe->writeSem);
            break;
        }

        semLock(&pipe->lock);
        pipe->buffer[pipe->writeIndex] = buffer[written];
        pipe->writeIndex = NEXT_IDX(pipe->writeIndex);
        semUnlock(&pipe->lock);

        if (post(pipe->readSem) != 0) {
            panic("Pipe read semaphore failed");
        }

        written++;
    }

    tryFinalizePipe(pipeID);
    return written;
}
