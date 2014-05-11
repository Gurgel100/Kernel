/*
 * lock.c
 *
 *  Created on: 29.07.2013
 *      Author: pascal
 */

#include "lock.h"

void lock(lock_t *l)
{
	asm volatile(
			"mov $1,%%dl;"
			".loop:"
			"xor %%al,%%al;"
			"lock cmpxchgb %%dl,(%0);"
			"jne .loop;"
			: : "r"(l) : "%al", "%dl");
}

void unlock(lock_t *l)
{
	*l = 0;
}

bool locked(lock_t *l)
{
	return *l;
}

void lock_wait(volatile lock_t *l)
{
	while(*l);
}

//Eine Variable atomar inkrementieren
void locked_inc(volatile uint64_t *var)
{
	asm volatile("lock incq (%0)" : : "r"(var));
}
