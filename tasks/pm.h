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

typedef struct process_t{
		context_t *Context;
		pid_t PID;
		struct process_t *parent;
		char *cmd;
		pm_status_t Status;
		console_t *console;
		list_t threads;

		void *nextThreadStack;
}process_t;

extern process_t *currentProcess;			//Aktueller Prozess

void pm_Init(void);
process_t *pm_InitTask(process_t *process, void *entry, char* cmd, bool newConsole);
void pm_DestroyTask(process_t *process);
void pm_ExitTask(uint64_t code);
void pm_BlockTask(process_t *process);
void pm_ActivateTask(process_t *process);
process_t *pm_getTask(pid_t PID);
console_t *pm_getConsole();

#endif /* PM_H_ */
