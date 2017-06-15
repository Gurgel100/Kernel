/*
 * dispatcher.h
 *
 *  Created on: 14.06.2017
 *      Author: pascal
 */

#ifndef DISPATCHER_H_
#define DISPATCHER_H_

#include <isr.h>

typedef void (*dispatcher_task_handler_t)(void *opaque);

void dispatcher_init();
void dispatcher_enqueue(dispatcher_task_handler_t handler, void *opaque);

ihs_t *dispatcher_dispatch(ihs_t *state);

#endif /* DISPATCHER_H_ */
