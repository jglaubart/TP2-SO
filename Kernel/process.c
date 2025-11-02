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

    process->pid = pid;
    process->ppid = parentID;
    process->priority = priority;
    process->state = PROCESS_STATE_READY;
    process->argc = argc;
    process->stack_base = stack_base;
    process->rip = function;
    process->argv = NULL;

    // copiar argumentos
    if (argc > 0) {
		process->argv = (char **) myMalloc(sizeof(char *) * process->argc);
		if (process->argv == NULL) {
            myFree(process);
            myFree(stack_base);
			return NULL;
		}

        for (int i = 0; i < argc; i++) {
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

    process->name = (process->argv != NULL && process->argc > 0) ? process->argv[0] : NULL;

    uint8_t * initial_rsp = stackInit(stack_top, function, process->argc, process->argv);
    if (initial_rsp == NULL) {
        if (process->argv != NULL) {
            for (int i = 0; i < process->argc; i++) {
                myFree(process->argv[i]);
            }
            myFree(process->argv);
        }
        myFree(process);
        myFree(stack_base);
        return NULL;
    }

    process->rsp = initial_rsp;

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

static int checkValidPid(int pid) {
    return (pid >= 0 && pid < MAX_PROCESSES);
}

int block(int pid) {
    if (pid == 0 || pid == 1 || !checkValidPid(pid)) {
        return -1;
    }
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
    if (pid == 0 || pid == 1 || !checkValidPid(pid)) {
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

int nice(int pid, int newPriority) {
    if (!checkValidPid(pid)) {
        return -1;
    }
    
    if (newPriority < MAX_PRIORITY || newPriority > MIN_PRIORITY) {
        return -1;
    }

    Process * process = getProcess(pid);
    if (process == NULL) {
        return -1;
    }

    process->priority = newPriority;
    return 0;
}

Process * getProcess(int pid) {
	if (PCBTable == NULL || !checkValidPid(pid)) {
		return NULL;
	}
	return PCBTable->processes[pid];
}


int getProcessInfo(int pid, ProcessInformation * info){
    if(info == NULL || !checkValidPid(pid)){
        return -1;
    }

    Process * process = getProcess(pid);
    if(process == NULL){
        return -1;
    }
    info->pid = process->pid;
    info->name = process->name;
    info->priority = process->priority;
    info->state = process->state;
    info->rsp = process->rsp;
    info->stack_base = process->stack_base;
    return 0;
}

int ps(ProcessInformation * processInfoTable){
    if(processInfoTable == NULL){
        return -1;
    }

    int count = 0;
    for(int pid = 0; pid < MAX_PROCESSES; pid++){
        if(getProcessInfo(pid, &processInfoTable[count]) == 0){
            count++;
        }
    }
    return count;
}
