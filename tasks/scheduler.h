/*
 * scheduler.h
 *
 *  Created on: 20.02.2015
 *      Author: pascal
 */

#ifndef SCHEDULER_H_
#define SCHEDULER_H_

#include "pm.h"
#include "thread.h"
#include "cpu.h"

extern process_t kernel_process;
extern thread_t *currentThread;
extern process_t *currentProcess;

void scheduler_Init();
void scheduler_activate();
void scheduler_add(thread_t *thread);
void scheduler_remove(thread_t *thread);

thread_t *scheduler_schedule(ihs_t *state);

void yield();

#endif /* SCHEDULER_H_ */
