/*
 * thread.h
 *
 *  Created on: 19.02.2015
 *      Author: pascal
 */

/**
 * \file
 * Contains all thread related stuff.
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

/**
 * \brief Structure describing a thread.
 */
typedef struct{
	/**
	 * The ring entry structure used for inserting the thread into the scheduler list.
	 */
	ring_entry_t ring_entry;

	/**
	 * The thread id (tid)
	 */
	tid_t tid;

	/**
	 * The process of the thread
	 */
	process_t *process;

	/**
	 * The state of the registers when the thread is not running.
	 */
	ihs_t *State;

	/**
	 * The state of the fpu, sse and avx registers.
	 */
	void *fpuState;

	/**
	 * The actual status of thread.
	 */
	thread_status_t Status;

	/**
	 * The bottom of the kernel stack.
	 */
	void *kernelStackBottom, *kernelStack;

	/**
	 * The bottom of the userspace stack.
	 */
	void *userStackBottom;

	/**
	 * Determines if the thread is the main thread of the process.
	 */
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
