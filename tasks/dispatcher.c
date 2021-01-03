/*
 * dispatcher.c
 *
 *  Created on: 14.06.2017
 *      Author: pascal
 */

#include "dispatcher.h"
#include <lock.h>
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef struct{
	dispatcher_task_handler_t func;
	void *opaque;
}dispatcher_task_t;

static lock_t dispatcher_lock = LOCK_INIT;
static dispatcher_task_t *queue;
static size_t queue_size;
static uint64_t queue_start, queue_end;

void dispatcher_init(size_t max_count)
{
	queue_size = max_count;
	queue = calloc(queue_size, sizeof(dispatcher_task_t));
	queue_start = queue_end = 0;
}

void dispatcher_enqueue(dispatcher_task_handler_t handler, void *opaque)
{
	LOCKED_TASK(dispatcher_lock, {
		if((queue_end >= queue_start && queue_end < queue_size)
				|| (queue_end < queue_start && queue_start > 0))
		{
			dispatcher_task_t *task = &queue[queue_end++];
			task->func = handler;
			task->opaque = opaque;
		}
		if(queue_end == queue_size && queue_start > 0)
			queue_end = 0;
	});
}

static ihs_t *dispatch_wrapper(ihs_t *state, uintptr_t ret, dispatcher_task_handler_t func, void *opaque)
{
	//Is mostly a no-op because the return value of the interrupt handler should not be modified
	*((uintptr_t*)state - 1) = ret;
	func(opaque);
	return state;
}

ihs_t *dispatcher_dispatch(ihs_t *state)
{
	static const ihs_t init_state = {
			.cs =  0x8,
			.ss = 0x10,
			.es = 0x10,
			.ds = 0x10,
			.gs = 0x10,
			.fs = 0x10,

			//IRQs einschalten (IF = 1)
			.rflags = 0x202,

			//Interrupt ist beim Schedulen 32
			.interrupt = 32
	};
	static ihs_t new_state;

	LOCKED_TRY_TASK(dispatcher_lock, {
		if(queue_start != queue_end)
		{
			dispatcher_task_t *task = &queue[queue_start++];

			if(queue_start == queue_size)
				queue_start = 0;

			memcpy(&new_state, &init_state, sizeof(ihs_t));
			new_state.rip = (uintptr_t)dispatch_wrapper;
			new_state.rsp = (uintptr_t)state - 8;

			new_state.rdi = (uintptr_t)state;
			new_state.rsi = *((uintptr_t*)state - 1);
			new_state.rdx = (uintptr_t)task->func;
			new_state.rcx = (uintptr_t)task->opaque;
			state = &new_state;
		}
	});

	return state;
}
