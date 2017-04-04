/*
 * ring.h
 *
 *  Created on: 19.02.2015
 *      Author: pascal
 */

#ifndef RING_H_
#define RING_H_

#include "stddef.h"
#include "stdbool.h"

/**
 * \brief Type of a ring entry.
 */
typedef struct ring_entry_s{

	/**
	 * Pointer to the next entry.
	 */
	struct ring_entry_s* next;

	/**
	 * Pointer to the previous entry.
	 */
	struct ring_entry_s* prev;
}ring_entry_t;

/**
 * \brief Type of a ring.
 */
typedef struct ring_s ring_t;

/**
 * \brief Creates a new ring.
 *
 * Creates and initializes a new ring. If an error occurs the function returns NULL.
 * @return pointer to a newly created and initialized ring or NULL
 */
ring_t *ring_create();

/**
 * \brief Destroys a ring.
 *
 * @param ring Ring to be destroyed
 */
void ring_destroy(ring_t *ring);

/**
 * \brief Returns the number of entries in a ring.
 *
 * Complexity: O(1)
 * @param ring Ring
 * @return number of entries in the ring
 */
size_t ring_entries(ring_t *ring);

/**
 * \brief Adds an entry to a ring.
 *
 * Complexity: O(1)
 * @param ring Ring where the entry shall be added
 * @param entry Entry to be added
 */
void ring_add(ring_t *ring, ring_entry_t *entry);

/**
 * \brief Removes an entry from a ring.
 *
 * Complexity: O(1)
 * @param ring Ring where the entry shall be removed
 * @param entry Entry to be removed
 */
void ring_remove(ring_t *ring, ring_entry_t *entry);

/**
 * \brief Gets the next entry of a ring.
 *
 * Sets the base pointer of a ring to the next entry and returns the entry pointed to.
 * Complexity: O(1)
 * @param ring Ring to be stepped
 * @return next entry in the ring
 */
ring_entry_t *ring_getNext(ring_t *ring);

/**
 * \brief Checks if an entry is contained a ring.
 *
 * Goes through the ring and searches for the entry.
 * Complexity: O(\#entries in ring)
 * @param ring Ring where the entry shall be searched
 * @param entry Entry to be checked
 * @return if entry is contained in ring
 */
bool ring_contains(ring_t *ring, ring_entry_t *entry);

#endif /* RING_H_ */
