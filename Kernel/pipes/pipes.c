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

int initPipes() {
    pipes = myMalloc(sizeof(pipeADT) * MAX_PIPES);
    if(pipes == NULL){
        panic("Failed to allocate memory for pipes");
    }
    memset(pipes, 0, sizeof(pipeADT) * MAX_PIPES);
    pipe_count = 0;
    strcpy(semNameBuffer, "pipeXXY");
    semNameBuffer[7] = '\0';
    return 0;
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


static void destroyPipe(pipeADT pipe) {
    if (pipe == NULL) {
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
    pipe->closed = 1;

    wakeBlocked(pipe->readSem);
    wakeBlocked(pipe->writeSem);

    destroyPipe(pipe);
    return 0;
}

