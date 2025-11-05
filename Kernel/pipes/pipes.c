#include "pipes.h"
#include <stdint.h>
#include <stddef.h>
#include "memory.h"
#include "panic.h"
#include "process.h"
#include "scheduler.h"
#include "lib.h"
#include "semaphores.h"

struct pipeCDT {
    int id;
    int readIndex;
    int writeIndex;
    uint8_t buffer[PIPE_BUFFER_SIZE];
    semADT readSem;
    semADT writeSem;
    int refCount;
};

static pipeADT * pipes = NULL;
static int pipe_count = 0;

// used a static buffer to avoid dynamic allocation in getSemName
static char semNameBuffer[16];
static char * getSemName() {
    semNameBuffer[4] = '0' + (pipe_count / 10);
    semNameBuffer[5] = '0' + (pipe_count % 10);
    return semNameBuffer;
}

int initPipes() {
    pipes = malloc(sizeof(pipeADT) * MAX_PIPES);
    if(pipes == NULL){
        panic("Failed to allocate memory for pipes");
    }
    strcpy(semNameBuffer, "pipe");
    semNameBuffer[7] = '\0';
}

static pipeADT buildPipe() {
    if(pipe_count >= MAX_PIPES){
        return NULL;
    }

    pipeADT newPipe = malloc(sizeof(struct pipeCDT));
    if(newPipe == NULL){
        return NULL;
    }

    newPipe->id = pipe_count;
    newPipe->readIndex = 0;
    newPipe->writeIndex = 0;
    newPipe->refCount = 0;

    // Initialize semaphores
    char * semName = getSemName();
    semName[6] = 'r';
    newPipe->readSem = semInit(semName, 0);
    if(newPipe->readSem == NULL){
        free(newPipe);
        return NULL;
    }
    semName[6] = 'w';
    newPipe->writeSem = semInit(semName, PIPE_BUFFER_SIZE);
    if(newPipe->writeSem == NULL){
        free(newPipe);
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

