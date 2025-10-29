#include "scheduler.h"
#include "panic.h"
#include "interrupts.h"

static void idleTask(void);

QueueADT readyQueue = NULL;

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

    if (enqueue(readyQueue, &idleProcess) == NULL) {
        panic("Failed to enqueue idle process.");
    }
}

static void idleTask(void) {
    while (1) {
        _hlt();
    }
}