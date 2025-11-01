#include "process.h"
#include <lib.h>
#include "memory.h"
#include "panic.h"
#include <string.h>
#include "scheduler.h"
#include "interrupts.h"

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

    PCBTable = myMalloc(sizeof(pcb_table));
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
        if (PCBTable->processes[pid] == NULL || PCBTable->processes[pid]->stack_base == 0 || PCBTable->processes[pid]->state == PROCESS_STATE_TERMINATED) {
            return pid;
        }
    }
    return -1;
}


Process * createProcess(void * function, int argc, char ** argv, int priority, int parentID){
    // validaciones iniciales
    if(function == NULL || argc < 0 || priority < 0){
        return NULL;
    }

    int pid = getNextPid();
    if (pid < 0 || pid >= MAX_PROCESSES) {
        return NULL;
    }

    // reservar espacio de stack para el proceso
    uint8_t * stack_base = myMalloc(PROCESS_STACK_SIZE);
    if (stack_base == NULL) {
        return NULL;
    }

    Process * process = myMalloc(sizeof(Process));
    if (process == NULL) {
        myFree(stack_base);
        return NULL;
    }

    uint8_t * stack_top = stack_base + PROCESS_STACK_SIZE - sizeof(uint64_t); //Stack crece hacia abajo

    uint8_t * initial_rsp = stackInit(stack_top, function, argc, argv);
    if (initial_rsp == NULL) {
        myFree(stack_base);
        myFree(process);
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
		process->argv = (char **) myMalloc(sizeof(char *) * process->argc);
		if (process->argv == NULL) {
			myFree(process);
            myFree(stack_base);
			return NULL;
		}

        for (int i = 0; i < process->argc; i++) {
            int len = strlen(argv[i]);
            process->argv[i] = (char *) myMalloc(sizeof(char) * (len + 1));
            // si cualquier reserva falla, liberar todo lo reservado hasta el momento
            if (process->argv[i] == NULL) {
                for (int j = 0; j < i; j++) {
                    myFree(process->argv[j]);
                }
                myFree(process->argv);
                myFree(process);
                myFree(stack_base);
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

    int added = addProcessToScheduler(process);
    if (added != 0) {
        freeProcess(process);
        return NULL;
    }
    
    return process;
}

void freeProcess(Process * p){
    if (p == NULL) {
        return;
    }

    if (PCBTable != NULL && p->pid >= 0 && p->pid < MAX_PROCESSES && PCBTable->processes[p->pid] == p) {
        // Eliminar proceso de la tabla (mantener contador coherente)
        PCBTable->processes[p->pid] = NULL;
        if (PCBTable->processesCount > 0) {
            PCBTable->processesCount--;
        }
    }

    p->state = PROCESS_STATE_TERMINATED;

    if (p->stack_base != NULL) {
        myFree(p->stack_base);
        p->stack_base = NULL;
    }

    for (int i = 0; i < p->argc; i++) {
        myFree(p->argv[i]);
    }
    myFree(p->argv);
    // Liberar memoria reservada para el proceso
    myFree(p);
}


int getProcessState(Process *process){
    if (process == NULL) {
        return -1;
    }
    return process->state;
}

int setProcessState(Process *process, int state){
    if (process == NULL) {
        return -1;
    }
    process->state = state;
    return 0;
}

int block(int pid) {
    Process * process = getProcess(pid);
    if (process == NULL) {
        return -1;
    }

    if (process->state == PROCESS_STATE_READY) {
        if (removeProcessFromScheduler(process) == -1) {
            return -1;
        }
        process->state = PROCESS_STATE_BLOCKED;
        return 0;
    }

    if (process->state == PROCESS_STATE_RUNNING) {
        process->state = PROCESS_STATE_BLOCKED;
        yield();
        return 0;
    }

    return -1;

}

int kill(int pid) {
    /* Protect critical system processes (idle=0 and shell/init=1) from being killed
       by arbitrary user processes to avoid freeing their stacks while they run. */
    if (pid == 0 || pid == 1) {
        return -1;
    }
    Process * process = getProcess(pid);
    if (process == NULL) {
        return -1;
    }
    
    removeProcessFromScheduler(process);

    // interrupcion para hacer context switch
    if(process->state == PROCESS_STATE_RUNNING) {
        yield();
    }
    
    process->state = PROCESS_STATE_TERMINATED;

    freeProcess(process);
    return 0;
}

int unblock(int pid) {
    Process * process = getProcess(pid);
    if (process == NULL) {
        return -1;
    }

    if(process->state != PROCESS_STATE_BLOCKED) {
        return -1;
    }
    process->state = PROCESS_STATE_READY;
    addProcessToScheduler(process);
    return 0;
}

Process * getProcess(int pid) {
	if (PCBTable == NULL || pid < 0 || pid >= MAX_PROCESSES) {
		return NULL;
	}
	return PCBTable->processes[pid];
}
