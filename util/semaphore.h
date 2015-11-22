/*
 * semaphore.h
 *
 *  Created on: 04.09.2015
 *      Author: pascal
 */

#ifndef SEMAPHORE_H_
#define SEMAPHORE_H_

#include "queue.h"
#include "lock.h"

typedef struct{
	int64_t count;
	queue_t *waiting;
	lock_t lock;
}semaphore_t;

void semaphore_init(semaphore_t *sem, int64_t count);
void semaphore_destroy(semaphore_t *sem);
void semaphore_acquire(semaphore_t *sem);
void semaphore_release(semaphore_t *sem);

#endif /* SEMAPHORE_H_ */
