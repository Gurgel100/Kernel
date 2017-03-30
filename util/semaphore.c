/*
 * semaphore.c
 *
 *  Created on: 04.09.2015
 *      Author: pascal
 */

#include "semaphore.h"
#include "thread.h"
#include "scheduler.h"

static int64_t get_and_add(volatile int64_t *val, int64_t add)
{
	int64_t ret;
	asm volatile("lock xaddq %%rax,%2" : "=a"(ret): "a"(add), "m"(*val));
	return ret;
}

void semaphore_init(semaphore_t *sem, int64_t count)
{
	if(sem == NULL)
		return;

	sem->lock = LOCK_UNLOCKED;
	sem->count = count;
	sem->waiting = queue_create();
}

void semaphore_destroy(semaphore_t *sem)
{
	if(sem == NULL)
		return;

	lock(&sem->lock);
	queue_destroy(sem->waiting);
}

void semaphore_acquire(semaphore_t *sem)
{
	if(sem == NULL)
		return;

retry:
	if(get_and_add(&sem->count, -1) <= 0)
	{
		lock(&sem->lock);
		queue_enqueue(sem->waiting, currentThread);
		unlock(&sem->lock);
		if(!thread_block_self(semaphore_release, sem)) goto retry;
		yield();
	}
}

void semaphore_release(semaphore_t *sem)
{
	if(sem == NULL)
		return;

	locked_inc((uint64_t*)&sem->count);
	if(queue_size(sem->waiting) > 0)
	{
		lock(&sem->lock);
		thread_unblock(queue_dequeue(sem->waiting));
		unlock(&sem->lock);
	}
}
