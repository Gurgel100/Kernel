/*
 * ring.h
 *
 *  Created on: 19.02.2015
 *      Author: pascal
 */

#ifndef RING_H_
#define RING_H_

#include "stddef.h"

typedef struct{
	void *value;
	void *next;
}ring_entry_t;

typedef struct{
	ring_entry_t *base;
	ring_entry_t *end;
	size_t size;
}ring_t;

ring_t *ring_create();
void ring_destroy(ring_t *ring);
void ring_add(ring_t *ring, void *value);
void *ring_get(ring_t *ring, size_t i);
void *ring_remove(ring_t *ring, size_t i);

#endif /* RING_H_ */
