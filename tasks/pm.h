/*
 * pm.h
 *
 *  Created on: 30.12.2012
 *      Author: pascal
 */

#ifndef PM_H_
#define PM_H_

#include "isr.h"
#include "stdint.h"
#include "vmm.h"

typedef uint64_t pid_t;

typedef struct{
		ihs_t *State;
		context_t *Context;
		pid_t PID;
		pid_t PPID;
		bool Active;
		bool Sleeping;
}process_t;

void pm_Init(void);
pid_t pm_InitTask(pid_t parent, void *entry);
void pm_DestroyTask(pid_t PID);
ihs_t *pm_ExitTask(ihs_t *cpu, uint64_t code);
void pm_HaltTask(pid_t PID);
void pm_ActivateTask(pid_t PID);
void pm_SleepTask(pid_t PID);
void pm_WakeTask(pid_t PID);
process_t *pm_getTask(pid_t PID);

#endif /* PM_H_ */
