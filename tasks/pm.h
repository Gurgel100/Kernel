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
#include "console.h"
#include "list.h"

typedef uint64_t pid_t;

typedef struct{
		uint64_t mmx[6];
		uint128_t ymm[16][2];		//YMM-Register sind 256-Bit breit
}__attribute__((aligned(16)))ihs_extended_t;

typedef enum{
	READY, BLOCKED, RUNNING, WAITING_USERIO
}pm_status_t;

typedef struct{
		context_t *Context;
		pid_t PID;
		pid_t PPID;
		char *cmd;
		pm_status_t Status;
		console_t *console;
		list_t threads;
}process_t;

extern process_t *currentProcess;			//Aktueller Prozess

void pm_Init(void);
pid_t pm_InitTask(pid_t parent, void *entry, char* cmd, bool newConsole);
void pm_DestroyTask(pid_t PID);
ihs_t *pm_ExitTask(ihs_t *cpu, uint64_t code);
void pm_BlockTask(pid_t PID);
void pm_ActivateTask(pid_t PID);
process_t *pm_getTask(pid_t PID);
console_t *pm_getConsole();

#endif /* PM_H_ */
