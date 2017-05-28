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

/**
 * \brief Type describing the reason why a thread is blocked
 */
typedef enum{
	/**
	 * Thread is not blocked
	 */
	THREAD_BLOCKED_NOT_BLOCKED,

	/**
	 * Thread is waiting for user input
	 */
	THREAD_BLOCKED_USER_IO,

	/**
	 * Thread is waiting for timer
	 */
	THREAD_BLOCKED_WAIT_TIMER,

	/**
	 * Thread is waiting for semaphore
	 */
	THREAD_BLOCKED_SEMAPHORE,

	/**
	 * Thread is waiting for unknown reason
	 */
	THREAD_BLOCKED_WAIT,

	/**
	 * Thread is blocked because the process is blocked
	 */
	THREAD_BLOCKED_PROCESS_BLOCKED,

	/**
	 * Thread is terminated
	 */
	THREAD_BLOCKED_TERMINATED
}thread_block_reason_t;

/**
 * \brief Type describing the state of a thread
 */
typedef enum{
	/**
	 * Thread is blocked
	 */
	THREAD_BLOCKED,

	/**
	 * Thread is running or is waiting for execution (is in scheduler queue)
	 */
	THREAD_RUNNING,

	/**
	 * Thread is terminated and is waiting for cleanup
	 */
	THREAD_TERMINATED
}thread_state_t;

typedef union{
	struct{
		thread_state_t status;
		thread_block_reason_t block_reason;
	};
	uint128_t full_status;
}thread_status_t __attribute__((aligned(16)));

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
	 * The actual status of thread.
	 */
	thread_status_t Status;

	/**
	 * The state of the fpu, sse and avx registers.
	 */
	void *fpuState;

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
bool thread_block(thread_t *thread, thread_block_reason_t reason);
bool thread_block_self(thread_bail_out_t bail, void *context, thread_block_reason_t reason);
bool thread_try_unblock(thread_t *thread);
void thread_unblock(thread_t *thread);
void thread_waitUserIO();

#endif /* THREAD_H_ */
