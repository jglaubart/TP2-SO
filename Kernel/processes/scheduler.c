// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "scheduler.h"
#include "panic.h"
#include "interrupts.h"
#include "queue.h"
#include "process.h"
#include "strings.h"
#include "memory.h"

#define STARVATION_THRESHOLD 5

static void idleTask(void);
static void validateScheduler(void);
static void enqueueReadyProcess(Process *process);
static Process *dequeueNextReadyProcess(void);
static int removeProcessFromQueue(QueueADT queue, Process *process);
static void ageWaitingPriorities(void);
static Process *tryDequeueAtPriority(int priority);

typedef struct scheduler {
    QueueADT readyQueues[MAX_PRIORITY + 1];
    Process * currentProcess;
    Process * idleProcess;
    int firstInterrupt;
    int currentQuantum;      // Current quantum counter for running process
    int quantumLimit;        // Quantum limit based on priority
    int starvationCounters[MAX_PRIORITY + 1];
    int starvationThreshold;
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
// Higher priority (higher number) = more time slices before context switch
static int getQuantumLimit(int priority) {
    // (left shift = multiply by 2^priority), to avoid importing pow
    return 1 << priority;
}

int initScheduler() {
    scheduler = myMalloc(sizeof(Scheduler));
    if (scheduler == NULL) {
        panic("Failed to allocate memory for Scheduler.");
    }

    for (int priority = MIN_PRIORITY; priority <= MAX_PRIORITY; priority++) {
        scheduler->readyQueues[priority] = createQueue((QueueElemCmpFn)compareProcessForRemoval, sizeof(Process *));
        if (scheduler->readyQueues[priority] == NULL) {
            panic("Failed to create ready queue for scheduler.");
        }
    }

    scheduler->currentQuantum = 0;
    scheduler->quantumLimit = 0;
    scheduler->starvationThreshold = STARVATION_THRESHOLD;
    for (int priority = MIN_PRIORITY; priority <= MAX_PRIORITY; priority++) {
        scheduler->starvationCounters[priority] = 0;
    }
    char ** idleArgv = myMalloc(sizeof(char *) * 2);
    idleArgv[0] = "idle";
    idleArgv[1] = NULL;
    Process *idleProcess = createProcess((void *)idleTask, 1, idleArgv, MIN_PRIORITY, -1, 1);
    if (idleProcess == NULL) {
        panic("Failed to create idle process.");
    }

    dequeue(scheduler->readyQueues[idleProcess->priority], &idleProcess); // Ensure idle process is not in the ready queue
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
                enqueueReadyProcess(scheduler->currentProcess);
            }
        } else {
            // Don't switch yet, continue with current process
            scheduler->firstInterrupt = 0;
            return scheduler->currentProcess->rsp;
        }
    }
    
    scheduler->firstInterrupt = 0;


    ageWaitingPriorities();
    Process *nextProcess = dequeueNextReadyProcess();
    if (nextProcess == NULL) {
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

    enqueueReadyProcess(process);

    return 0;
}

int removeProcessFromScheduler(Process *process) {
    validateScheduler();

    if (process == NULL) {
        return -1;
    }

    if (process->priority >= MIN_PRIORITY && process->priority <= MAX_PRIORITY) {
        if (removeProcessFromQueue(scheduler->readyQueues[process->priority], process) == 0) {
            return 0;
        }
    }

    for (int priority = MIN_PRIORITY; priority <= MAX_PRIORITY; priority++) {
        if (priority == process->priority) {
            continue;
        }
        if (removeProcessFromQueue(scheduler->readyQueues[priority], process) == 0) {
            return 0;
        }
    }

    return -1;
}

int schedulerRequeueReadyProcess(Process *process) {
    validateScheduler();

    if (process == NULL) {
        return -1;
    }

    if (process->state != PROCESS_STATE_READY) {
        return 0;
    }

    if (removeProcessFromScheduler(process) != 0) {
        return -1;
    }

    enqueueReadyProcess(process);
    return 0;
}

void yield(void) { _force_timer_interrupt(); }

static void idleTask(void) {
    while (1) {
        _hlt();
    }
}

static void validateScheduler() {
    if (scheduler == NULL) {
        panic("Scheduler not initialized.");
    }

    for (int priority = MIN_PRIORITY; priority <= MAX_PRIORITY; priority++) {
        if (scheduler->readyQueues[priority] == NULL) {
            panic("Scheduler ready queues not initialized.");
        }
    }
}

static void enqueueReadyProcess(Process *process) {
    if (process == NULL) {
        panic("Cannot enqueue NULL process.");
    }

    int priority = process->priority;
    if (priority < MIN_PRIORITY || priority > MAX_PRIORITY) {
        panic("Process priority out of bounds.");
    }

    if (enqueue(scheduler->readyQueues[priority], &process) == NULL) {
        panic("Failed to enqueue process to scheduler.");
    }
}

static Process *dequeueNextReadyProcess(void) {
    for (int priority = MAX_PRIORITY; priority >= MIN_PRIORITY; priority--) {
        if (scheduler->starvationCounters[priority] >= scheduler->starvationThreshold) {
            Process *boosted = tryDequeueAtPriority(priority);
            if (boosted != NULL) {
                return boosted;
            }
        }
    }

    for (int priority = MAX_PRIORITY; priority >= MIN_PRIORITY; priority--) {
        Process *next = tryDequeueAtPriority(priority);
        if (next != NULL) {
            return next;
        }
    }

    return NULL;
}

static Process *tryDequeueAtPriority(int priority) {
    QueueADT queue = scheduler->readyQueues[priority];
    if (queue != NULL && !queueIsEmpty(queue)) {
        Process *next = NULL;
        if (dequeue(queue, &next) != NULL) {
            scheduler->starvationCounters[priority] = 0;
            return next;
        }
    }
    return NULL;
}

static int removeProcessFromQueue(QueueADT queue, Process *process) {
    if (queue == NULL || process == NULL || queueIsEmpty(queue)) {
        return -1;
    }

    Process *target = process;
    return queueRemove(queue, &target) == NULL ? -1 : 0;
}

static void ageWaitingPriorities(void) {
    for (int priority = MIN_PRIORITY; priority <= MAX_PRIORITY; priority++) {
        QueueADT queue = scheduler->readyQueues[priority];
        if (queue != NULL && !queueIsEmpty(queue)) {
            if (scheduler->starvationCounters[priority] < scheduler->starvationThreshold) {
                scheduler->starvationCounters[priority]++;
            }
        } else {
            scheduler->starvationCounters[priority] = 0;
        }
    }
}
