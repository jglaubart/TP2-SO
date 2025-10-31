#include "process.h"
#include <lib.h>
#include "memory.h"
#include "panic.h"
#include <string.h>

typedef struct pcb_table {
    Process * processes[MAX_PROCESSES];
    int processesCount;
    int current_pid;
} pcb_table;

static pcb_table * PCBTable = NULL;

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
		PCBTable->processes[i] = NULL;
	}

    return 0;
}


int getNextPid(void) {
    // si es el primer proceso (idle), pid 0.
    if(PCBTable->current_pid < 0){
        return 0;
    }
    
    int start = PCBTable->current_pid + 1;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        int pid = (start + i) % MAX_PROCESSES;
        if (PCBTable->processes[pid]->stack_base == 0 || PCBTable->processes[pid]->state == PROCESS_STATE_TERMINATED) {
            return pid;
        }
    }
    return -1;
}


Process * createProcess(uint8_t * function, int argc, char ** argv, int priority, int parentID){
    // validaciones iniciales
    if(function == NULL || argc < 0 || priority < 0){
        return NULL;
    }

    int pid = getNextPid();
    if (pid < 0 || pid >= MAX_PROCESSES) {
        return NULL;
    }

    // reservar espacio de stack para el proceso
    uint8_t * stack_base = malloc(PROCESS_STACK_SIZE);
    if (stack_base == NULL) {
        return NULL;
    }

    Process * process = malloc(sizeof(Process));
    if (process == NULL) {
        free(stack_base);
        return NULL;
    }

    uint8_t * stack_top = stack_base + PROCESS_STACK_SIZE - sizeof(uint64_t); //Stack crece hacia abajo

    uint8_t * initial_rsp = stackInit(stack_top, function, argc, argv);
    if (initial_rsp == NULL) {
        free(stack_base);
        free(process);
        return NULL;
    }

    process->pid = pid;
    process->ppid = parentID;
    process->name = (argv != NULL && argc > 0) ? argv[0] : NULL;
    process->priority = priority;
    process->state = PROCESS_STATE_READY;
    process->argc = argc;
    process->stack_base = stack_base;
    process->rip = function;
    process->rsp = initial_rsp;

    // copiar argumentos
    if (argc > 0) {
		process->argv = (char **) malloc(sizeof(char *) * process->argc);
		if (process->argv == NULL) {
			free(process);
            free(stack_base);
			return NULL;
		}

        for (int i = 0; i < process->argc; i++) {
            int len = strlen(argv[i]);
            process->argv[i] = (char *) malloc(sizeof(char) * (len + 1));
            // si cualquier reserva falla, liberar todo lo reservado hasta el momento
            if (process->argv[i] == NULL) {
                for (int j = 0; j < i; j++) {
                    free(process->argv[j]);
                }
                free(process->argv);
                free(process);
                free(stack_base);
                return NULL;
            }
            strcpy(process->argv[i], argv[i]);
            process->argv[i][len] = '\0';
	    }
	}
    else {
        process->argv = NULL;
    }

    // agregar proceso a la PCB
    PCBTable->processes[pid] = process;
    PCBTable->processesCount++;
    PCBTable->current_pid = pid;
    return process;
}

void freeProcess(Process * p){
    if (p == NULL) {
        return;
    }

    if (p->stack_base != NULL) {
        free(p->stack_base);
        p->stack_base = NULL;
    }

    for (int i = 0; i < p->argc; i++) {
		free(p->argv[i]);
	}
	free(p->argv);
	free(p);

    p->state = PROCESS_STATE_TERMINATED;
    PCBTable->processesCount--;
}


