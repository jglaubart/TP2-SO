#include "scheduler.h"
#include "panic.h"
#include "interrupts.h"
#include "queue.h"

static void idleTask(void);

QueueADT readyQueue = NULL;
static Process *currentProcess = NULL;

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

void initScheduler() {
    readyQueue = createQueue((QueueElemCmpFn)compareProcessPriority, sizeof(Process *));
    if (readyQueue == NULL) {
        panic("Failed to create ready queue for scheduler.");
    }

    Process *idleProcess = createProcess((uint8_t *)idleTask, 0, NULL, 0, -1);
    if (idleProcess == NULL) {
        panic("Failed to create idle process.");
    }

    idleProcess->name = "idle";
    currentProcess = idleProcess;
    
    if (enqueue(readyQueue, &idleProcess) == NULL) {
        panic("Failed to enqueue idle process.");
    }
}

uint8_t *schedule(uint8_t *rsp) {
    if (currentProcess != NULL) {
        currentProcess->rsp = rsp;
        if (currentProcess->state == PROCESS_STATE_RUNNING) {
            currentProcess->state = PROCESS_STATE_READY;
            if (enqueue(readyQueue, &currentProcess) == NULL) {
                panic("Failed to re-enqueue current process.");
            }
        }
    }

    Process *nextProcess = NULL;
    if (dequeue(readyQueue, &nextProcess) == NULL || nextProcess == NULL) {
        panic("No process available to schedule.");
    }

    nextProcess->state = PROCESS_STATE_RUNNING;
    currentProcess = nextProcess;

    return currentProcess->rsp;
}

static void idleTask(void) {
    while (1) {
        _hlt();
    }
}