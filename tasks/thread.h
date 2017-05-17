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
#include "ring.h"

typedef uint64_t tid_t;

typedef enum{
	THREAD_BLOCKED, THREAD_RUNNING
}thread_status_t;

typedef struct{
	ring_entry_t ring_entry;
	tid_t tid;
	process_t *process;
	ihs_t *State;
	void *fpuState;
	thread_status_t Status;
	void *kernelStackBottom, *kernelStack;
	void *userStackBottom;
	bool isMainThread;
}thread_t;

typedef void(*thread_bail_out_t)(void*);

void thread_Init();
thread_t *thread_create(process_t *process, void *entry, size_t data_length, void *data, bool kernel);
void thread_destroy(thread_t *thread);
void thread_prepare(thread_t *thread);
bool thread_block_self(thread_bail_out_t bail, void *context);
bool thread_try_unblock(thread_t *thread);
void thread_unblock(thread_t *thread);
void thread_waitUserIO();

#endif /* THREAD_H_ */
