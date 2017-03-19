/*
 * lock.h
 *
 *  Created on: 29.07.2013
 *      Author: pascal
 */

#ifndef LOCK_H_
#define LOCK_H_

#include "stdint.h"
#include "stdbool.h"

#define LOCK_LOCKED		1
#define LOCK_UNLOCKED	0

typedef uint64_t lock_t;

//Führt eine Funktion gelockt aus und gibt deren Resultat zurück
#define LOCKED_RESULT(lck, task)\
	({\
		lock(&lck);\
		typeof(task) ___result = task;\
		unlock(&lck);\
		___result;\
	})

//Führt eine Funktion gelockt aus
#define LOCKED_TASK(lck, task)\
	{\
		lock(&lck);\
		task;\
		unlock(&lck);\
	}

bool try_lock(lock_t *l);
void lock(lock_t *l);
void unlock(lock_t *l);
bool locked(lock_t *l);
void lock_wait(volatile lock_t *l);

//Eine Variable atomar inkrementieren
void locked_inc(volatile uint64_t *var);
//Eine Variable atomar inkrementieren
void locked_dec(volatile uint64_t *var);

#endif /* LOCK_H_ */
