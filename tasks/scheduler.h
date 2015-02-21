/*
 * scheduler.h
 *
 *  Created on: 20.02.2015
 *      Author: pascal
 */

#ifndef SCHEDULER_H_
#define SCHEDULER_H_

#include "thread.h"

void scheduler_Init();
void scheduler_add(thread_t *thread);
void scheduler_remove(thread_t *thread);

thread_t *scheduler_schedule();

#endif /* SCHEDULER_H_ */
