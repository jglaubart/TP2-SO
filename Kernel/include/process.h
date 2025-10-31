#ifndef PROCESS_H
#define PROCESS_H
#include <stdint.h>

#define PROCESS_STACK_SIZE 4096
#define MAX_PROCESSES 64
#define IDLE_PROCESS_PID 0
#define INIT_PROCESS_PID 1
#define SHELL_PROCESS_PID 2
#define MAX_PRIORITY 0
#define MIN_PRIORITY 4
#define MID_PRIORITY 2


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

int startFirstProcess(void);

#endif
