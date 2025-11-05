#ifndef PROCESS_H
#define PROCESS_H
#include <stdint.h>
#include "queue.h"
#include "semaphores.h"

#define PROCESS_STACK_SIZE 4096
#define MAX_PROCESSES 64
#define IDLE_PROCESS_PID 0
#define INIT_PROCESS_PID 1
#define SHELL_PROCESS_PID 2
#define PROCESS_NAME_MAX_LENGTH 64

typedef enum {
    MIN_PRIORITY = 0,
    MID_PRIORITY = 1,
    MAX_PRIORITY = 2
} ProcessPriority;

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
    int waiting_for_child; // PID of child this process is waiting for (-1 if not waiting)
    uint8_t is_background; // 1 if the process was launched in background mode
    uint8_t is_foreground; // 1 if the process currently owns the foreground
    QueueADT children; // Queue of child PIDs
    semADT wait_sem; // Semaphore for waiting on child processes
} Process;

typedef struct ProcessInformation{
    int pid;
    char name[PROCESS_NAME_MAX_LENGTH];
    int priority;
    ProcessState state;
    uint8_t * rsp;
    uint8_t * stack_base;
    uint8_t is_foreground;
} ProcessInformation;

int getNextPid(void);
Process * createProcess(void * function, int argc, char ** argv, ProcessPriority priority, int parentID, uint8_t is_background);
void removeProcess(Process * p);
void freeProcess(Process * p);
void processCleanupTerminated(Process *exclude);
int initPCBTable();

int kill(int pid);
int block(int pid);
int unblock(int pid);
int nice(int pid, int newPriority);
int waitPid(int pid);
int waitChildren(void);
Process * getProcess(int pid);
int getProcessInfo(int pid, ProcessInformation * info);
Process * getForegroundProcess(void);
void setForegroundProcess(Process * process);
void releaseForegroundProcess(Process * process);
int killForegroundProcess(void);
int startInitProcess(void * shellEntryPoint);
int ps(ProcessInformation * processInfoTable); // Always recieves a table of MAX_PROCESSES size
int changePriority(int pid, ProcessPriority newPriority);
int getCurrentPid();


#endif
