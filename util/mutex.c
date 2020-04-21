/*
 * mutex.c
 *
 *  Created on: 08.11.2017
 *      Author: pascal
 */

#include "mutex.h"
#include "assert.h"
#include "scheduler.h"

void mutex_init(mutex_t *mutex)
{
	assert(mutex != NULL);
	mutex->owner = 0;
	mutex->waiting = queue_create();
	mutex->lock = LOCK_INIT;
}

void mutex_destroy(mutex_t *mutex)
{
	assert(mutex != NULL);
	// Set garbage so that the mutex can no longer be used
	static lock_node_t lock_node;
	lock(&mutex->lock, &lock_node);
	queue_destroy(mutex->waiting);
}

void mutex_lock(mutex_t *mutex)
{
	assert(mutex != NULL);

	tid_t self = currentThread->tid;

	while(!__sync_bool_compare_and_swap(&mutex->owner, 0, self))
	{
		LOCKED_TASK(mutex->lock, queue_enqueue(mutex->waiting, currentThread));
		while(!thread_block_self((thread_bail_out_t)mutex_unlock, mutex, THREAD_BLOCKED_MUTEX));
	}
}

int mutex_unlock(mutex_t *mutex)
{
	assert(mutex != NULL);

	tid_t self = currentThread->tid;

	if(!__sync_bool_compare_and_swap(&mutex->owner, self, 0))
		return 1;

	LOCKED_TASK(mutex->lock, thread_unblock(queue_dequeue(mutex->waiting)));

	return 0;
}
