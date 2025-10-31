#ifndef PROCESS_H
#define PROCESS_H
#include <stdint.h>

#define PROCESS_STACK_SIZE 4096
#define MAX_PROCESSES 64

typedef enum ProcessState {
    PROCESS_STATE_READY,
    PROCESS_STATE_RUNNING,
    PROCESS_STATE_BLOCKED,
    PROCESS_STATE_TERMINATED
} ProcessState;

typedef struct Process {
    int pid;
    int ppid;
    char * name;
    int priority;
    ProcessState state;
    int argc;
    char ** argv;
    uint8_t * stack_base;
    uint8_t * rip;   //function
    uint8_t * rsp;
} Process;

int getNextPid(void);
Process * createProcess(void * function, int argc, char ** argv, int priority, int parentID);
void freeProcess(Process * p);
int initPCBTable();

int kill(int pid);
int block(int pid);
int unblock(int pid);
Process * getProcess(int pid);

#endif
