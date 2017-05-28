/*
 * refcount.c
 *
 *  Created on: 21.03.2017
 *      Author: pascal
 */

#include "refcount.h"
#include "assert.h"

void refcount_init(void *obj, size_t offset, void (*free)(const void*))
{
	refcount_t *o = (refcount_t*)((uintptr_t)obj + offset);
	o->ref_count = 1;
	o->free = free;
}

void *refcount_retain(void *obj, size_t offset)
{
	refcount_t *o = (refcount_t*)((uintptr_t)obj + offset);
	uint64_t val, oldval;

	assert(o->ref_count > 0);

	do
	{
		oldval = o->ref_count;
		if(oldval == 0) return NULL;
		val = __sync_val_compare_and_swap(&o->ref_count, oldval, oldval + 1);
	}
	while(val != oldval && val != 0);

	if(val == 0)
		return NULL;
	else
		return obj;
}

bool refcount_release(void *obj, size_t offset)
{
	refcount_t *o = (refcount_t*)((uintptr_t)obj + offset);
	if(__sync_add_and_fetch(&o->ref_count, -1) == 0 && o->free != NULL)
	{
		o->free(obj);
		return true;
	}
	return false;
}
