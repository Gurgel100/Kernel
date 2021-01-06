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

void lock(lock_t *lock, lock_node_t *node) {
    node->next = NULL;
    node->locked = true;

    lock_node_t *prev = __atomic_exchange_n(lock, node, __ATOMIC_ACQ_REL);
    if (prev != NULL) {
        __atomic_store_n(&prev->next, node, __ATOMIC_RELEASE);
        while (__atomic_load_n(&node->locked, __ATOMIC_ACQUIRE)) pause();
    }
}

void unlock(lock_t *lock, lock_node_t *node) {
    if (__atomic_load_n(&node->next, __ATOMIC_ACQUIRE) == NULL) {
        lock_node_t *expected = node;
        if (__atomic_compare_exchange_n(lock, &expected, NULL, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
            // Nobody waiting
            return;
        }

        // Wait until someone is definitely enqueued
        while (__atomic_load_n(&node->next, __ATOMIC_ACQUIRE) == NULL) pause();
    }
    // Release lock
    __atomic_store_n(&node->next->locked, false, __ATOMIC_RELEASE);
}

bool try_lock(lock_t *lock, lock_node_t *node) {
    node->next = NULL;
    lock_node_t *expected = NULL;
    return __atomic_compare_exchange_n(lock, &expected, node, false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED);
}

void lock_wait(lock_t *lock) {
    while (__atomic_load_n(lock, __ATOMIC_RELAXED) != NULL) pause();
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
