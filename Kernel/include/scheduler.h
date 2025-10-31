#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "queue.h"
#include "process.h"

int initScheduler();
uint8_t * schedule(uint8_t *rsp);
int addProcessToScheduler(Process *process);
int removeProcessFromScheduler(Process *process);

#endif
