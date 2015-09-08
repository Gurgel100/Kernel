/*
 * queue.h
 *
 *  Created on: 31.08.2015
 *      Author: pascal
 */

#ifndef QUEUE_H_
#define QUEUE_H_

#include "stddef.h"

typedef struct queue_entry_s{
	void *value;
	struct queue_entry_s *next;
}queue_entry_t;

typedef struct{
	queue_entry_t *begin;
	queue_entry_t *end;
	size_t size;
}queue_t;

queue_t *queue_create();
void queue_destroy(queue_t *queue);
size_t queue_size(queue_t* queue);
void *queue_enqueue(queue_t *queue, void *value);
void *queue_dequeue(queue_t *queue);

#endif /* QUEUE_H_ */
