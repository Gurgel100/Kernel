/*
 * ring.c
 *
 *  Created on: 19.02.2015
 *      Author: pascal
 */

#include "ring.h"
#include "stdlib.h"

/*
 * Erstellt einen neuen Ring
 *
 * Rückgabewert:	Pointer auf den Ring
 */
ring_t *ring_create()
{
	ring_t *ring = calloc(1, sizeof(ring_t));

	return ring;
}

/*
 * Zerstört einen Ring
 *
 * Parameter:	ring = Der freizugebender Ring
 */
void ring_destroy(ring_t *ring)
{
	if(ring == NULL)
		return;

	while(ring->size--)
	{
		ring_remove(ring, 0);
	}

	free(ring);
}

/*
 * Gibt die Grösse des Rings zurück.
 *
 * Parameter:	Ring, dessen Grösse zurückgegeben werden soll
 *
 * Rückgabe:	Grösse des Rings
 */
size_t ring_size(ring_t* ring)
{
	if(ring != NULL)
		return ring->size;
	return 0;
}

/*
 * Fügt ein Element zu einem Ring hinzu.
 *
 * Parameter:	ring = Der Ring, in das das Element eingefügt werden soll
 * 				value = Wert des Elements
 * Rückgabe:	Wert des eingefügten Elements
 */
void* ring_add(ring_t* ring, void* value)
{
	if(ring == NULL)
		return NULL;

	ring_entry_t *entry = malloc(sizeof(ring_entry_t));
	if(entry == NULL)
		return NULL;

	entry->value = value;

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

	return (void*)entry;
}

/*
 * Gibt das nächste Element des Rings zurück.
 *
 * Parameter:	ring = Ring
 * Rückgabe:	Wert des Elements
 */
void* ring_getNext(ring_t* ring)
{
	ring_entry_t* entry;

	if(ring == NULL || ring->size == 0)
		return NULL;

	entry = ring->base;
	ring->base = ring->base->next;

	return entry->value;
}

/*
 * Entfernt das übergebene Element aus dem Ring.
 *
 * Parameter:		ring = Ring, in dem das Element ist
 * 					element = Element, das gelöscht werden soll
 * Rückgabe:	Wert des gelöschten Elements
 */
void* ring_remove(ring_t* ring, void* element)
{
	void *val;
	ring_entry_t *entry = element;

	if(ring == NULL || ring->size == 0 || entry == NULL)
		return NULL;

	ring->size--;

	val = entry->value;
	entry->next->prev = entry->prev;
	entry->prev->next = entry->next;
	free(entry);

	return val;
}


/*
 * Durchsucht den Ring nach dem ersten Element mit dem übergebenem Wert.
 *
 * Parameter:	ring = Ring, in dem das Element gesucht werden soll
 * 				value = Wert, nach dem gesucht werden soll
 * Rückgabe:	Das gesuchte Element oder NULL, wenn nichts gefunden wurde
 */
void* ring_find(ring_t* ring, void* value)
{
	ring_entry_t* entry;

	if(ring == NULL || ring->size == 0)
		return NULL;

	entry = ring->base;
	do
	{
		if(entry->value == value)
			return entry;
		entry = entry->next;
	}
	while(entry != ring->base);

	return NULL;
}
