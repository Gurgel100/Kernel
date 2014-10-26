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

void pm_Init(void);
pid_t pm_InitTask(pid_t parent, void *entry);
void pm_DestroyTask(pid_t PID);
void pm_HaltTask(pid_t PID);
void pm_SleepTask(pid_t PID);
void pm_WakeTask(pid_t PID);

#endif /* PM_H_ */
