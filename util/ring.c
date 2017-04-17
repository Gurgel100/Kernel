/*
 * ring.c
 *
 *  Created on: 19.02.2015
 *      Author: pascal
 */

#include "ring.h"
#include "stdlib.h"
#include "assert.h"

struct ring_s{
	ring_entry_t* base;
	size_t size;
};

ring_t *ring_create()
{
	ring_t *ring = calloc(1, sizeof(ring_t));

	return ring;
}

void ring_destroy(ring_t *ring)
{
	assert(ring != NULL);
	free(ring);
}

size_t ring_entries(ring_t *ring)
{
	assert(ring != NULL);
	return ring->size;
}

void ring_add(ring_t *ring, ring_entry_t *entry)
{
	assert(ring != NULL);
	assert(entry != NULL);

	if(ring->size)
	{
		entry->next = ring->base;
		entry->prev = ring->base->prev;
		ring->base->prev->next = entry;
		ring->base->prev = entry;
	}
	else
	{
		ring->base = entry;
		entry->next = entry->prev = entry;
	}
	ring->size++;
}

ring_entry_t *ring_getNext(ring_t* ring)
{
	ring_entry_t* entry;

	assert(ring != NULL);

	if(ring->size == 0)
		return NULL;

	entry = ring->base;
	ring->base = ring->base->next;

	return entry;
}

void ring_remove(ring_t* ring, ring_entry_t *entry)
{
	assert(ring != NULL);
	assert(entry != NULL);

	if(ring->size == 0)
		return;

	ring->size--;

	entry->next->prev = entry->prev;
	entry->prev->next = entry->next;
	if(ring->size == 0)
		ring->base = NULL;
	else if(ring->base == entry)
		ring->base = entry->next;
}


bool ring_contains(ring_t *ring, ring_entry_t *entry)
{
	assert(ring != NULL);
	assert(entry != NULL);

	if(ring->size == 0)
		return false;

	ring_entry_t *e = ring->base;
	do
	{
		assert(e != NULL);
		if(e == entry)
			return true;
		e = e->next;
	}
	while(e != ring->base);

	return false;
}
