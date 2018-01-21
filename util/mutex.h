/*
 * mutex.h
 *
 *  Created on: 08.11.2017
 *      Author: pascal
 */

#ifndef MUTEX_H_
#define MUTEX_H_

#include "thread.h"
#include "lock.h"
#include "queue.h"

typedef struct{
	tid_t owner;
	queue_t *waiting;
	lock_t lock;
}mutex_t;

void mutex_init(mutex_t *mutex);
void mutex_destroy(mutex_t *mutex);
void mutex_lock(mutex_t *mutex);
int mutex_unlock(mutex_t *mutex);

#endif /* MUTEX_H_ */
