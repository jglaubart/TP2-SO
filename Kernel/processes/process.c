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

static Process *terminatedQueue[MAX_PROCESSES];
static int terminatedCount = 0;

static void enqueueTerminatedProcess(Process *process) {
    if (process == NULL) {
        return;
    }

    for (int i = 0; i < terminatedCount; i++) {
        if (terminatedQueue[i] == process) {
            return;
        }
    }

    if (terminatedCount >= MAX_PROCESSES) {
        panic("Terminated process queue overflow");
    }

    terminatedQueue[terminatedCount++] = process;
}

void processCleanupTerminated(Process *exclude) {
    if (terminatedCount == 0) {
        return;
    }

    int writeIndex = 0;

    for (int i = 0; i < terminatedCount; i++) {
        Process *process = terminatedQueue[i];
        if (process == NULL) {
            continue;
        }

        if (process == exclude) {
            terminatedQueue[writeIndex++] = process;
            continue;
        }

        freeProcess(process);
    }

    for (int i = writeIndex; i < terminatedCount; i++) {
        terminatedQueue[i] = NULL;
    }

    terminatedCount = writeIndex;
}

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

int changePriority(int pid, ProcessPriority newPriority) {
    if (pid < 0 || pid >= MAX_PROCESSES || newPriority < MIN_PRIORITY || newPriority > MAX_PRIORITY) {
        return -1;
    }

    Process * process = PCBTable->processes[pid];
    if (process == NULL) {
        return -1;
    }

    process->priority = newPriority;
    return 0;
}

Process * createProcess(void * function, int argc, char ** argv, ProcessPriority priority, int parentID, uint8_t is_background){
    // validaciones iniciales
    if(function == NULL || argc < 0 || priority < 0 || (argc > 0 && argv == NULL)){
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
    process->is_background = is_background;
    process->waiting_for_child = -1;

    process->argv = (char **) myMalloc(sizeof(char *) * (process->argc + 1));
    if (process->argv == NULL) {
        myFree(process);
        myFree(stack_base);
        return NULL;
    }

    for (int i = 0; i < argc; i++) {
        int len = strlen(argv[i]);
        process->argv[i] = (char *) myMalloc(sizeof(char) * (len + 1));
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
    process->argv[argc] = NULL;

    process->name = (process->argc > 0) ? process->argv[0] : NULL;

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

    if (p->argv != NULL) {
        for (int i = 0; i < p->argc; i++) {
            myFree(p->argv[i]);
        }
        myFree(p->argv);
        p->argv = NULL;
    }
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
    if (pid == IDLE_PROCESS_PID || !checkValidPid(pid)) {
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
    
    ProcessState previousState = process->state;
    if (previousState == PROCESS_STATE_TERMINATED) {
        return 0;
    }

    if (previousState == PROCESS_STATE_READY) {
        if (removeProcessFromScheduler(process) == -1) {
            return -1;
        }
    }

    // Mark process as terminated
    process->state = PROCESS_STATE_TERMINATED;

    // Wake up parent if waiting for this child
    if (process->ppid >= 0 && checkValidPid(process->ppid)) {
        Process * parent = getProcess(process->ppid);
        if (parent != NULL && parent->waiting_for_child == pid) {
            parent->waiting_for_child = -1;
            if (parent->state == PROCESS_STATE_BLOCKED) {
                unblock(parent->pid);
            }
        }
    }

    if (previousState == PROCESS_STATE_RUNNING) {
        enqueueTerminatedProcess(process);
        // interrupcion para hacer context switch
        yield();
        return 0;
    }

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
    
    if (newPriority < MIN_PRIORITY || newPriority > MAX_PRIORITY) {
        return -1;
    }

    Process * process = getProcess(pid);
    if (process == NULL) {
        return -1;
    }

    process->priority = newPriority;
    return 0;
}

int waitPid(int pid) {
    // Validate target PID
    if (!checkValidPid(pid)) {
        return -1;
    }

    Process * child = getProcess(pid);
    if (child == NULL) {
        return -1; // Child process doesn't exist
    }

    Process * current = getCurrentProcess();
    if (current == NULL) {
        return -1;
    }

    // Verify that the target is actually a child of the caller
    if (child->ppid != current->pid) {
        return -1; // Not a child of the calling process
    }

    // If child is already terminated, return immediately
    if (child->state == PROCESS_STATE_TERMINATED) {
        return 0;
    }

    // Mark that we're waiting for this child
    current->waiting_for_child = pid;

    // Block the current process
    if (block(current->pid) == -1) {
        current->waiting_for_child = -1;
        return -1;
    }

    // When we get here, we've been unblocked (child terminated)
    current->waiting_for_child = -1;
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

    _cli();
    Process * process = getProcess(pid);
    if(process == NULL){
        _sti();
        return -1;
    }

    info->pid = process->pid;

    if (process->name != NULL) {
        strncpy(info->name, process->name, PROCESS_NAME_MAX_LENGTH - 1);
        info->name[PROCESS_NAME_MAX_LENGTH - 1] = '\0';
    } else {
        info->name[0] = '\0';
    }

    info->priority = process->priority;
    info->state = process->state;
    info->rsp = process->rsp;
    info->stack_base = process->stack_base;
    _sti();
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
