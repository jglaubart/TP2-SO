#include "process.h"
#include <lib.h>
#include "memory.h"
#include "panic.h"

typedef struct pcb_table {
    Process processes[MAX_PROCESSES];
    int processesCount;
    int current_pid;
} pcb_table;

static pcb_table *PCBTable = NULL;

int initPCBTable() {
    if (PCBTable != NULL) {
		panic("PCB already initialized");
		return -1;	// PCB already initialized
	}

    PCBTable = malloc(sizeof(pcb_table));
    if (PCBTable == NULL) {
        panic("Failed to allocate memory for PCB Table");
        return -1; // Memory allocation failed 
    }

    PCBTable->processesCount = 0;
    PCBTable->current_pid = -1;

    for (int i = 0; i < MAX_PROCESSES; i++) {
        PCBTable->processes[i] = (Process){0}; // Mark all process slots as free
    }

    return 0;
}


int getNextPid(void) {
    // si es el primer proceso (idle), pid 0.
    if(PCBTable->current_pid < 0){
        return 0;
    }
    
    int start = PCBTable->current_pid;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        int pid = (start + i) % MAX_PROCESSES;
        if (PCBTable->processes[pid].stack_base == 0 || PCBTable->processes[pid].state == PROCESS_STATE_TERMINATED) {
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

    Process * process = &PCBTable->processes[pid];

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

void freeProcess(Process * p){
    
}