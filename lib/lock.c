/*
 * lock.c
 *
 *  Created on: 29.07.2013
 *      Author: pascal
 */

#include "lock.h"

bool try_lock(lock_t *l)
{
	return __sync_bool_compare_and_swap(l, 0, 1);
}

void lock(lock_t *l)
{
	while(!try_lock(l))
		asm volatile("pause");
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

//Eine Variable atomar inkrementieren
void locked_dec(volatile uint64_t *var)
{
	asm volatile("lock decq (%0)" : : "r"(var));
}
