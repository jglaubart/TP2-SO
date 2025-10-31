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
} Scheduler;

static Scheduler *scheduler = NULL;

static int compareProcessPriority(void *a, void *b) {
    Process *procA = (Process *)a;
	Process *procB = (Process *)b;

    if (procA->priority > procB->priority) {
		return 1;
	} else if (procA->priority < procB->priority) {
		return -1;
	} else {
		return 0;
	}
}

int initScheduler() {
    if (scheduler != NULL) {
        panic("scheduler already initialized");
        return -1;	// scheduler already initialized
    }

    scheduler = myMalloc(sizeof(Scheduler));
    if (scheduler == NULL) {
        panic("Failed to allocate memory for Scheduler.");
    }

    scheduler->readyQueue = createQueue((QueueElemCmpFn)compareProcessPriority, sizeof(Process *));
    if (scheduler->readyQueue == NULL) {
        panic("Failed to create ready queue for scheduler.");
    }
    char ** idleArgv = myMalloc(sizeof(char *) * 2);
    idleArgv[0] = "idle";
    idleArgv[1] = NULL;
    Process *idleProcess = createProcess((void *)idleTask, 1, idleArgv, MIN_PRIORITY, -1);
    if (idleProcess == NULL) {
        panic("Failed to create idle process.");
    }

    int x = dequeue(scheduler->readyQueue, &idleProcess); // Ensure idle process is not in the ready queue
    idleProcess->state = PROCESS_STATE_RUNNING;
    scheduler->currentProcess = idleProcess;
    scheduler->idleProcess = idleProcess;

    return 0;
}

uint8_t *schedule(uint8_t *rsp) {
    if (scheduler->currentProcess != NULL) {
        scheduler->currentProcess->rsp = rsp;
        if (scheduler->currentProcess->state == PROCESS_STATE_RUNNING) {
            scheduler->currentProcess->state = PROCESS_STATE_READY;
            if (enqueue(scheduler->readyQueue, &scheduler->currentProcess) == NULL) {
                panic("Failed to re-enqueue current process.");
            }
        }
    }

    Process *nextProcess = NULL;
    if (dequeue(scheduler->readyQueue, &nextProcess) == NULL || nextProcess == NULL) {
        panic("No process available to schedule.");
    }

    nextProcess->state = PROCESS_STATE_RUNNING;
    scheduler->currentProcess = nextProcess;

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
