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
#include "stddef.h"

#define LOCK_INIT	NULL

//Führt eine Funktion gelockt aus und gibt deren Resultat zurück
#define LOCKED_RESULT(lck, task)			\
	({										\
        lock_node_t ___lock_node; 			\
		lock(&lck, &___lock_node);			\
		typeof((task)) ___result = (task);	\
		unlock(&lck, &___lock_node);		\
		___result;							\
	})

//Führt eine Funktion gelockt aus
#define LOCKED_TASK(lck, task)				\
	{										\
        lock_node_t ___lock_node; 			\
		lock(&lck, &___lock_node);			\
		task;								\
		unlock(&lck, &___lock_node);		\
	}

//Versucht den lock zu holen und führt dann den task aus
#define LOCKED_TRY_TASK(lck, task)			\
	{										\
        lock_node_t ___lock_node;			\
		if (try_lock(&lck, &___lock_node))	\
		{									\
			task;							\
			unlock(&lck, &___lock_node);	\
		}									\
	}

typedef struct lock_node {
    struct lock_node *next;
    bool locked;
} lock_node_t __attribute__((aligned(64)));

typedef lock_node_t *lock_t;

bool try_lock(lock_t *lock, lock_node_t *node);
void lock(lock_t *lock, lock_node_t *node);
void unlock(lock_t *lock, lock_node_t *node);
bool locked(const lock_t *lock);
void lock_wait(lock_t *lock);

#endif /* LOCK_H_ */
