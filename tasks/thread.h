/*
 * thread.h
 *
 *  Created on: 19.02.2015
 *      Author: pascal
 */

#ifndef THREAD_H_
#define THREAD_H_

#include "stdint.h"
#include "isr.h"
#include "pm.h"
#include "stdbool.h"

typedef uint64_t tid_t;

typedef struct{
	tid_t tid;
	process_t * process;
	ihs_t *State;
	void *fpuState;
	pm_status_t Status;
	void *kernelStackBottom, *kernelStack;
	void *userStackBottom, *userStackPhys;
	bool isMainThread;
}thread_t;

void thread_Init();
thread_t *thread_create(process_t *process, void *entry, size_t data_length, void *data, bool kernel);
void thread_destroy(thread_t *thread);
void thread_prepare(thread_t *thread);
void thread_block(thread_t *thread);
void thread_unblock(thread_t *thread);
void thread_waitUserIO(thread_t* thread);

#endif /* THREAD_H_ */
