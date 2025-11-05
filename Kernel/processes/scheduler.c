#include "scheduler.h"
#include "panic.h"
#include "interrupts.h"
#include "queue.h"
#include "process.h"
#include "strings.h"
#include "memory.h"

static void idleTask(void);
static void validateScheduler(void);

typedef struct scheduler {
    QueueADT readyQueue;
    Process * currentProcess;
    Process * idleProcess;
    int firstInterrupt;
    int currentQuantum;      // Current quantum counter for running process
    int quantumLimit;        // Quantum limit based on priority
} Scheduler;

static Scheduler *scheduler = NULL;

// Comparison function for finding processes (used by queueRemove)
// Only compares process pointers for equality, NOT for priority ordering
static int compareProcessForRemoval(void *a, void *b) {
    Process *procA = (a == NULL) ? NULL : *(Process **)a;
    Process *procB = (b == NULL) ? NULL : *(Process **)b;

    if (procA == procB) {
        return 0;
    }
    // Return non-zero if different (doesn't matter which value)
    return (procA > procB) ? 1 : -1;
}

// Helper function to calculate quantum limit based on priority
// Higher priority (lower number) = more time slices before context switch
static int getQuantumLimit(int priority) {
    // (right shift = divide by 2^priority), to avoid importing pow
    return 4 >> priority;
}

int initScheduler() {
    scheduler = myMalloc(sizeof(Scheduler));
    if (scheduler == NULL) {
        panic("Failed to allocate memory for Scheduler.");
    }

    scheduler->readyQueue = createQueue((QueueElemCmpFn)compareProcessForRemoval, sizeof(Process *));
    if (scheduler->readyQueue == NULL) {
        panic("Failed to create ready queue for scheduler.");
    }

    scheduler->currentQuantum = 0;
    scheduler->quantumLimit = 0;
    char ** idleArgv = myMalloc(sizeof(char *) * 2);
    idleArgv[0] = "idle";
    idleArgv[1] = NULL;
    Process *idleProcess = createProcess((void *)idleTask, 1, idleArgv, MIN_PRIORITY, -1, 1);
    if (idleProcess == NULL) {
        panic("Failed to create idle process.");
    }

    dequeue(scheduler->readyQueue, &idleProcess); // Ensure idle process is not in the ready queue
    idleProcess->state = PROCESS_STATE_RUNNING;
    scheduler->currentProcess = idleProcess;
    scheduler->idleProcess = idleProcess;
    scheduler->firstInterrupt = 1;

    return 0;
}

uint8_t *schedule(uint8_t *rsp) {
    processCleanupTerminated(scheduler == NULL ? NULL : scheduler->currentProcess);

    if (scheduler->currentProcess != NULL) {
        // On the first interrupt, we're still in kernel context, not in the idle process context.
        // Don't overwrite the idle process's properly initialized stack frame.
        // Only save RSP if this is not the first interrupt, or if we're switching from a process that has actually run.
        if (!scheduler->firstInterrupt || scheduler->currentProcess != scheduler->idleProcess) {
            scheduler->currentProcess->rsp = rsp;
        }
        
        // Increment quantum counter
        scheduler->currentQuantum++;
        
        // Check if we should switch: either quantum expired or process is not RUNNING
        int shouldSwitch = (scheduler->currentProcess->state != PROCESS_STATE_RUNNING) ||
                          (scheduler->currentQuantum >= scheduler->quantumLimit);
        
        if (shouldSwitch) {
            if (scheduler->currentProcess->state == PROCESS_STATE_RUNNING) {
                scheduler->currentProcess->state = PROCESS_STATE_READY;
                if (enqueue(scheduler->readyQueue, &scheduler->currentProcess) == NULL) {
                    panic("Failed to re-enqueue current process.");
                }
            }
        } else {
            // Don't switch yet, continue with current process
            scheduler->firstInterrupt = 0;
            return scheduler->currentProcess->rsp;
        }
    }
    
    scheduler->firstInterrupt = 0;

    
    Process *nextProcess = NULL;
    if (dequeue(scheduler->readyQueue, &nextProcess) == NULL || nextProcess == NULL) {
        /*
         * No ready process available in the ready queue. Previously this
         * triggered a panic which halts the system. Instead of panicking,
         * fallback to the idle process so the scheduler remains robust when
         * there are temporarily no runnable processes.
         */
        nextProcess = scheduler->idleProcess;
        if (nextProcess == NULL) {
            panic("No process available to schedule.");
        }
    }

    nextProcess->state = PROCESS_STATE_RUNNING;
    scheduler->currentProcess = nextProcess;
    
    // Reset quantum counter and set limit based on new process priority
    scheduler->currentQuantum = 0;
    scheduler->quantumLimit = getQuantumLimit(nextProcess->priority);

    return scheduler->currentProcess->rsp;
}

Process * getCurrentProcess() {
    validateScheduler();
    return scheduler->currentProcess;
}

int addProcessToScheduler(Process * process) {
    validateScheduler();

    if (process == NULL) {
        return -1;
    }

    if (enqueue(scheduler->readyQueue, &process) == NULL) {
        panic("Failed to enqueue process to scheduler.");
    }

    return 0;
}

int removeProcessFromScheduler(Process *process) {
    validateScheduler();

    if (process == NULL) {
        return -1;
    }

    if (queueIsEmpty(scheduler->readyQueue)) {
        return -1;
    }

    Process *target = process;
    if (queueRemove(scheduler->readyQueue, &target) == NULL) {
        return -1;
    }

    return 0;
}

void yield(void) { _force_timer_interrupt(); }

static void idleTask(void) {
    while (1) {
        _hlt();
    }
}

static void validateScheduler() {
    if (scheduler == NULL || scheduler->readyQueue == NULL) {
        panic("Scheduler not initialized.");
    }
}
