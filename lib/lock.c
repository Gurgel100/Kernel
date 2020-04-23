/*
 * lock.c
 *
 *  Created on: 29.07.2013
 *      Author: pascal
 */

#include "lock.h"

#define pause() asm volatile("pause")

bool locked(const lock_t *lock) {
    return *lock == NULL;
}

void lock(volatile lock_t *lock, volatile lock_node_t *node) {
    node->next = NULL;

    lock_node_t *prev;
    do {
        prev = *lock;
    } while (!__sync_bool_compare_and_swap(lock, prev, node));
    if (prev != NULL) {
        node->locked = true;
        prev->next = node;
        while (node->locked) pause();
    }
}

void unlock(volatile lock_t *lock, volatile lock_node_t *node) {
    if (node->next == NULL) {
        if (__sync_bool_compare_and_swap(lock, node, NULL)) {
            // Nobody waiting
            return;
        }

        // Wait until someone is definitely enqueued
        while (node->next == NULL) pause();
    }
    // Release lock
    node->next->locked = false;
}

bool try_lock(lock_t *lock, lock_node_t *node) {
    node->next = NULL;
    return __sync_bool_compare_and_swap(lock, NULL, node);
}

void lock_wait(volatile lock_t *lock) {
    while (*lock != NULL) pause();
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
