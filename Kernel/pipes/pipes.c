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
    int activeOps;
};

static pipeADT * pipes = NULL;
static int pipeSerial = 0;

// used a static buffer to avoid dynamic allocation in getSemName
static char semNameBuffer[16];
static char * getSemName(int serial, char mode) {
    semNameBuffer[4] = '0' + ((serial / 10) % 10);
    semNameBuffer[5] = '0' + (serial % 10);
    semNameBuffer[6] = mode;
    return semNameBuffer;
}

void initPipes() {
    pipes = myMalloc(sizeof(pipeADT) * MAX_PIPES);
    if(pipes == NULL){
        panic("Failed to allocate memory for pipes");
    }

    pipeSerial = 0;
    strcpy(semNameBuffer, "pipeXXY");
    semNameBuffer[7] = '\0';
    for (int i = 0; i < MAX_PIPES; i++) {
        pipes[i] = NULL;
    }
}

static void freePipe(pipeADT pipe) {
    if (pipe == NULL) {
        return;
    }
    semDestroy(pipe->readSem);
    semDestroy(pipe->writeSem);
    myFree(pipe);
}

static pipeADT buildPipe(int slot, int serial) {
    pipeADT newPipe = myMalloc(sizeof(struct pipeCDT));
    if(newPipe == NULL){
        return NULL;
    }

    newPipe->id = slot;
    newPipe->readIndex = 0;
    newPipe->writeIndex = 0;
    newPipe->refCount = 0;
    newPipe->closed = 0;
    newPipe->lock = 0;
    newPipe->activeOps = 0;

    // Initialize semaphores
    getSemName(serial, 'R');
    newPipe->readSem = semInit(semNameBuffer, 0);
    if(newPipe->readSem == NULL){
        myFree(newPipe);
        return NULL;
    }
    getSemName(serial, 'W');
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
    int slot = -1;
    for (int i = 0; i < MAX_PIPES; i++) {
        if (pipes[i] == NULL) {
            slot = i;
            break;
        }
    }
    if (slot == -1) {
        return -1;
    }

    int serial = pipeSerial;
    pipeADT pipe = buildPipe(slot, serial);
    if(pipe == NULL){
        return -1;
    }
    pipeSerial++;
    pipes[slot] = pipe;
    if (pipeRetain(pipe->id) != 0) {
        pipes[slot] = NULL;
        freePipe(pipe);
        return -1;
    }
    return pipe->id;
}

static pipeADT getPipe(int pipeID) {
    if (pipeID < 0 || pipeID >= MAX_PIPES) {
        return NULL;
    }
    return pipes[pipeID];
}

static void tryFinalizePipe(int pipeID) {
    pipeADT pipe = getPipe(pipeID);
    if (pipe == NULL) {
        return;
    }
    semLock(&pipe->lock);
    int shouldFinalize = pipe->closed && pipe->refCount == 0 && pipe->activeOps == 0;
    if (!shouldFinalize) {
        semUnlock(&pipe->lock);
        return;
    }
    pipes[pipe->id] = NULL;
    semUnlock(&pipe->lock);

    freePipe(pipe);
}

static void pipeEnterOperation(pipeADT pipe) {
    semLock(&pipe->lock);
    pipe->activeOps++;
    semUnlock(&pipe->lock);
}

static void pipeLeaveOperation(pipeADT pipe) {
    semLock(&pipe->lock);
    pipe->activeOps--;
    semUnlock(&pipe->lock);
}



int closePipe(int pipeID) {
    pipeADT pipe = getPipe(pipeID);
    if (pipe == NULL) {
        return -1;
    }

    int shouldWake = FALSE;
    int remainingRefs;
    semLock(&pipe->lock);
    if (pipe->refCount > 0) {
        pipe->refCount--;
    }
    remainingRefs = pipe->refCount;
    if (remainingRefs == 0 && !pipe->closed) {
        pipe->closed = TRUE;
        shouldWake = TRUE;
    }
    semUnlock(&pipe->lock);

    if (remainingRefs > 0) {
        return 0;
    }

    if (shouldWake) {
        wakeBlocked(pipe->readSem);
        wakeBlocked(pipe->writeSem);
    }

    tryFinalizePipe(pipeID);
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

    pipeEnterOperation(pipe);
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

    pipeLeaveOperation(pipe);
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

    pipeEnterOperation(pipe);
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

    pipeLeaveOperation(pipe);
    tryFinalizePipe(pipeID);
    return written;
}

int pipeRetain(int pipeID) {
    pipeADT pipe = getPipe(pipeID);
    if (pipe == NULL) {
        return -1;
    }

    semLock(&pipe->lock);
    if (pipe->closed) {
        semUnlock(&pipe->lock);
        return -1;
    }
    pipe->refCount++;
    semUnlock(&pipe->lock);
    return 0;
}

int pipeRelease(int pipeID) {
    return closePipe(pipeID);
}

void pipeResetEndpoints(PipeEndpoint endpoints[PIPE_FD_COUNT]) {
    if (endpoints == NULL) {
        return;
    }

    for (int i = 0; i < PIPE_FD_COUNT; i++) {
        endpoints[i].type = PIPE_ENDPOINT_CONSOLE;
        endpoints[i].pipeID = -1;
    }
}

static int pipeSetEndpoint(PipeEndpoint *endpoint, PipeEndpointType type, int pipeID) {
    if (endpoint == NULL) {
        return -1;
    }

    if (endpoint->type == PIPE_ENDPOINT_PIPE && endpoint->pipeID >= 0) {
        pipeRelease(endpoint->pipeID);
    }

    endpoint->type = PIPE_ENDPOINT_NONE;
    endpoint->pipeID = -1;

    switch (type) {
        case PIPE_ENDPOINT_NONE:
            return 0;
        case PIPE_ENDPOINT_CONSOLE:
            endpoint->type = PIPE_ENDPOINT_CONSOLE;
            return 0;
        case PIPE_ENDPOINT_PIPE:
            if (pipeRetain(pipeID) != 0) {
                return -1;
            }
            endpoint->type = PIPE_ENDPOINT_PIPE;
            endpoint->pipeID = pipeID;
            return 0;
        default:
            return -1;
    }
}

int pipeSetReadTarget(PipeEndpoint endpoints[PIPE_FD_COUNT], PipeEndpointType type, int pipeID) {
    if (endpoints == NULL) {
        return -1;
    }
    return pipeSetEndpoint(&endpoints[READ_FD], type, pipeID);
}

int pipeSetWriteTarget(PipeEndpoint endpoints[PIPE_FD_COUNT], PipeEndpointType type, int pipeID) {
    if (endpoints == NULL) {
        return -1;
    }
    return pipeSetEndpoint(&endpoints[WRITE_FD], type, pipeID);
}

int pipeReadEndpoint(PipeEndpoint *endpoint, uint8_t *buffer, int size) {
    if (endpoint == NULL) {
        return -1;
    }

    if (endpoint->type != PIPE_ENDPOINT_PIPE) {
        return -1;
    }

    return readPipe(endpoint->pipeID, buffer, size);
}

int pipeWriteEndpoint(PipeEndpoint *endpoint, const uint8_t *buffer, int size) {
    if (endpoint == NULL) {
        return -1;
    }

    if (endpoint->type != PIPE_ENDPOINT_PIPE) {
        return -1;
    }

    // cast away const to reuse writePipe signature
    return writePipe(endpoint->pipeID, (uint8_t *)buffer, size);
}
