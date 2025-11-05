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
    int foreground_pid;
} pcb_table;

static pcb_table * PCBTable = NULL;

static Process *terminatedQueue[MAX_PROCESSES];
static int terminatedCount = 0;
static void * init_shell_entry = NULL;
static int initProcessMain(void);

static int cmpInt(void *a, void *b) {
    return *((int *)a) - *((int *)b);
}

static void initProcessSem(Process *process) {
    char semName[16] = "process";
	semName[7] = '0' + process->pid / 100;
	semName[8] = '0' + process->pid / 10;
	semName[9] = '0' + process->pid % 10;
	semName[10] = '\0';
	process->wait_sem = semInit(semName, 0);
}

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

        removeProcess(process);
    }

    for (int i = writeIndex; i < terminatedCount; i++) {
        terminatedQueue[i] = NULL;
    }

    terminatedCount = writeIndex;
}

int initPCBTable() {
    if (PCBTable != NULL) {
		panic("PCB already initialized");
	}

    PCBTable = myMalloc(sizeof(pcb_table));
    if (PCBTable == NULL) {
        panic("Failed to allocate memory for PCB Table");
    }

    PCBTable->processesCount = 0;
    PCBTable->current_pid = -1;
    PCBTable->foreground_pid = -1;

    for (int i = 0; i < MAX_PROCESSES; i++) {
		PCBTable->processes[i] = NULL;
	}

    return 0;
}

int startInitProcess(void * shellEntryPoint) {
    if (shellEntryPoint == NULL) {
        return -1;
    }

    init_shell_entry = shellEntryPoint;

    char * initArgv[] = { "init", NULL };
    Process * initProcess = createProcess((void *)initProcessMain, 1, initArgv, MID_PRIORITY, -1, 1);
    if (initProcess == NULL) {
        return -1;
    }

    return initProcess->pid;
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
    process->is_foreground = 0;
    process->waiting_for_child = -1;
    process->children = createQueue(cmpInt, sizeof(int));
    if(process->children == NULL){
        myFree(process);
        myFree(stack_base);
        return NULL;
    }
    initProcessSem(process);

    process->argv = (char **) myMalloc(sizeof(char *) * (process->argc + 1));
    if (process->argv == NULL) {
        myFree(process);
        myFree(stack_base);
        semDestroy(process->wait_sem);
        queueFree(process->children);
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
            semDestroy(process->wait_sem);
            queueFree(process->children);
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
        semDestroy(process->wait_sem);
        queueFree(process->children);
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

    if (parentID >= 0) {
        Process * parent = getProcess(parentID);
        if (parent != NULL && parent->children != NULL) {
            enqueue(parent->children, &process->pid);
        }
    }

    if (!is_background) {
        setForegroundProcess(process);
    }
    
    return process;
}

static int initProcessMain(void) {
    while (1) {
        if (init_shell_entry == NULL) {
            panic("Init process without shell entry");
        }

        char * shellArgv[] = { "shell", NULL };
        Process * shell = createProcess(init_shell_entry, 1, shellArgv, MID_PRIORITY, INIT_PROCESS_PID, 0);
        if (shell == NULL) {
            panic("Failed to launch shell process");
        }

        waitPid(shell->pid);
    }

    return 0;
}

void removeProcess(Process * p){
    if (p == NULL) {
        return;
    }
    releaseForegroundProcess(p);
    p->state = PROCESS_STATE_TERMINATED;

    semDestroy(p->wait_sem);

    if (PCBTable != NULL && p->pid >= 0 && p->pid < MAX_PROCESSES && PCBTable->processes[p->pid] == p) {
        PCBTable->processes[p->pid] = NULL;
        if (PCBTable->processesCount > 0) {
            PCBTable->processesCount--;
        }
    }

    // The children of the terminated process are adopted by the process' parent.
    Process * parent = getProcess(p->ppid);
    if (parent != NULL && parent->children != NULL) {
        queueRemove(parent->children, &p->pid);
    }
    if (parent == NULL || parent->state == PROCESS_STATE_TERMINATED) {
        parent = getProcess(INIT_PROCESS_PID);
    }

    if (parent == NULL || parent->state == PROCESS_STATE_TERMINATED) {
        parent = getProcess(SHELL_PROCESS_PID);
    }

    if (parent == NULL || parent->state == PROCESS_STATE_TERMINATED) {
        parent = getProcess(IDLE_PROCESS_PID);
    }

    while(!queueIsEmpty(p->children)) {
        int child_pid;
        dequeue(p->children, &child_pid);
        Process *child = getProcess(child_pid);
        if (child != NULL && parent != NULL) {
            child->ppid = parent->pid;
            if (parent->children != NULL) {
                enqueue(parent->children, &child->pid);
            }
        }
    }
	
    freeProcess(p);
}

void freeProcess(Process * p){
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
    queueFree(p->children);
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
    if (!checkValidPid(pid)) {
        return -1;
    }
    Process * p = getProcess(pid);
    if (p == NULL) {
        return -1;
    }

    if (p->state == PROCESS_STATE_READY) {
        if (removeProcessFromScheduler(p) == -1) {
            return -1;
        }
        p->state = PROCESS_STATE_BLOCKED;
        return 0;
    }

    if (p->state == PROCESS_STATE_RUNNING) {
        p->state = PROCESS_STATE_BLOCKED;
        yield();
        return 0;
    }

    return -1;

}

int kill(int pid) {
    /* Protect critical system processes (idle/init/shell) from being killed
       by arbitrary user processes to avoid freeing their stacks while they run. */
    if (!checkValidPid(pid)) {
        return -1;
    }

    if (pid == IDLE_PROCESS_PID || pid == INIT_PROCESS_PID || pid == SHELL_PROCESS_PID) {
        return -1;
    }

    Process * process = getProcess(pid);
    if (process == NULL) {
        return -1;
    }
    
    releaseForegroundProcess(process);
    
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
    if (previousState == PROCESS_STATE_RUNNING) {
        enqueueTerminatedProcess(process);
        // interrupcion para hacer context switch
        yield();
        return 0;
    }

    removeProcess(process);
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
    Process * p = getProcess(pid);
    if (p == NULL) {
        return -1; // Child process doesn't exist
    }

    Process * current = getCurrentProcess();
    if (current == NULL) {
        return -1;
    }

    // If child is already terminated, return immediately
    if (p->state == PROCESS_STATE_TERMINATED) {
        if (current->children != NULL) {
            queueRemove(current->children, &pid);
        }
        return 0;
    }
    if(p->wait_sem == NULL) {
        return -1;
    }
    int waitResult = wait(p->wait_sem);
    if (current->children != NULL) {
        queueRemove(current->children, &pid);
    }
    return waitResult;
}

int waitChildren(void) {
    Process * current = getCurrentProcess();
    if (current == NULL) {
        return -1;
    }

    if (current->children == NULL || queueIsEmpty(current->children)) {
        return 0;
    }

    int childCount = queueSize(current->children);
    if (childCount <= 0) {
        return 0;
    }

    if (childCount > MAX_PROCESSES) {
        childCount = MAX_PROCESSES;
    }

    int childPids[MAX_PROCESSES];

    if (queueBeginCyclicIter(current->children) == NULL) {
        return 0;
    }

    for (int i = 0; i < childCount; i++) {
        queueNextCyclicIter(current->children, &childPids[i]);
    }

    int result = 0;

    for (int i = 0; i < childCount; i++) {
        int pid = childPids[i];
        Process * child = getProcess(pid);
        if (child == NULL) {
            queueRemove(current->children, &pid);
            continue;
        }

        if (child->state == PROCESS_STATE_TERMINATED) {
            queueRemove(current->children, &pid);
            continue;
        }

        if (child->wait_sem == NULL) {
            queueRemove(current->children, &pid);
            result = -1;
            continue;
        }

        if (wait(child->wait_sem) != 0) {
            result = -1;
        }

        queueRemove(current->children, &pid);
    }

    return result;
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
    info->is_foreground = process->is_foreground;
    _sti();
    return 0;
}

Process * getForegroundProcess(void) {
    if (PCBTable == NULL) {
        return NULL;
    }

    _cli();
    int pid = PCBTable->foreground_pid;
    Process *process = NULL;
    if (pid >= 0 && pid < MAX_PROCESSES) {
        process = PCBTable->processes[pid];
    }
    _sti();
    return process;
}

void setForegroundProcess(Process * process) {
    if (PCBTable == NULL) {
        return;
    }

    _cli();
    int currentPid = PCBTable->foreground_pid;
    if (currentPid >= 0 && currentPid < MAX_PROCESSES) {
        Process *current = PCBTable->processes[currentPid];
        if (current != NULL) {
            current->is_foreground = 0;
        }
    }

    if (process != NULL) {
        PCBTable->foreground_pid = process->pid;
        process->is_foreground = 1;
    } else {
        PCBTable->foreground_pid = -1;
    }
    _sti();
}

void releaseForegroundProcess(Process * process) {
    if (process == NULL) {
        return;
    }

    Process *current = getForegroundProcess();
    if (current == NULL || current->pid != process->pid) {
        return; //el proceso a matar no es foreground
    }

    Process *candidate = getProcess(process->ppid);
    if (candidate == NULL || candidate->state == PROCESS_STATE_TERMINATED) {
        candidate = getProcess(INIT_PROCESS_PID);
    }

    if (candidate == NULL || candidate->state == PROCESS_STATE_TERMINATED) {
        candidate = getProcess(SHELL_PROCESS_PID);
    }

    if (candidate == NULL || candidate->state == PROCESS_STATE_TERMINATED) {
        candidate = getProcess(IDLE_PROCESS_PID);
    }

    setForegroundProcess(candidate);
}

int killForegroundProcess(void) {
    Process *foreground = getForegroundProcess();
    if (foreground == NULL) {
        return -1;
    }

    if (foreground->pid <= SHELL_PROCESS_PID) {
        // Never kill idle/init/shell via Ctrl+C
        return -1;
    }

    return kill(foreground->pid);
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

int getCurrentPid() {
    Process * current = getCurrentProcess();
    if (current == NULL) {
        return -1;
    }
    return current->pid;
}
