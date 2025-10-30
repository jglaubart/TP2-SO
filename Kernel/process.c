#include "process.h"
#include <lib.h>
#include "memory.h"

int getNextPid(void) {
    for (int pid = 0; pid < MAX_PROCESSES; pid++) {
        if (PCBTable[pid].stack_base == 0 || PCBTable[pid].state == PROCESS_STATE_TERMINATED) {
            return pid;
        }
    }
    return -1;
}

Process * createProcess(uint8_t * function, int argc, char ** argv, int priority, int parentID){
    int pid = getNextPid();
    if (pid < 0) {
        return NULL;
    }

    uint8_t * stack_base = malloc(PROCESS_STACK_SIZE);
    if (stack_base == NULL) {
        return NULL;
    }

    uint8_t * stack_top = stack_base + PROCESS_STACK_SIZE - sizeof(uint64_t); //Stack crece hacia abajo

    uint8_t * initial_rsp = stackInit(stack_top, function, argc, argv);
    if (initial_rsp == NULL) {
        free(stack_base);
        return NULL;
    }

    Process * process = &PCBTable[pid];
    memset(process, 0, sizeof(Process));

    process->pid = pid;
    process->ppid = parentID;
    process->name = (argv != NULL && argc > 0) ? argv[0] : NULL;
    process->priority = priority;
    process->state = PROCESS_STATE_READY;
    process->argc = argc;
    process->argv = argv;
    process->stack_base = stack_base;
    process->rip = function;
    process->rsp = initial_rsp;

    
    return process;
}