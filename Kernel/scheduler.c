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

    Process *idleProcess = createProcess((uint8_t *)idleTask, 0, NULL, 0, -1);
    if (idleProcess == NULL) {
        panic("Failed to create idle process.");
    }

    idleProcess->name = "idle";
    scheduler->currentProcess = idleProcess;

    addProcessToScheduler(idleProcess);

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

int addProcessToScheduler(Process *process) {
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
