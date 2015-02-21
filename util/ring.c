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
 * Fügt ein Element zu einem Ring hinzu.
 *
 * Parameter:	ring = Der Ring, in das das Element eingefügt werden soll
 * 				value = Wert des Elements
 */
void ring_add(ring_t *ring, void *value)
{
	if(ring == NULL)
		return;

	ring_entry_t *entry = malloc(sizeof(entry));
	if(entry == NULL)
		return;

	entry->value = value;

	entry->next = ring->base;
	ring->base = entry;

	if(entry->next == NULL)
	{
		//Erster Eintrag
		entry->next = entry;
		ring->end = entry;
	}
	else
		ring->end->next = entry;
}

/*
 * Gibt das Element an der Stelle i zurück. Wenn i grösser als die Anzahl Einträge im Ring ist,
 * dann wird einfach weitergezählt, so dass man schlussendlich das Element an der Stelle i % size zurück bekommt.
 *
 * Parameter:	ring = Ring in dem das Element steht
 * 				i = Index
 * Rückgabe:	Wert des Elements
 */
void *ring_get(ring_t *ring, size_t i)
{
	ring_entry_t *entry;

	if(ring == NULL || ring->size == 0)
		return NULL;

	entry = ring->base;
	while(i--)
		entry = entry->next;

	return entry->value;
}

/*
 * Entfernt das Element an der Stelle i % size (siehe oben).
 *
 * Parameter:		ring = Ring, in dem das Element ist
 * 					i = Index
 * Rückgabewert:	Wert des Elements
 */
void *ring_remove(ring_t *ring, size_t i)
{
	ring_entry_t *entry, *prev;
	void *val;

	if(ring == NULL || ring->size == 0)
		return NULL;

	if(ring->size == 1)
	{
		val = ring->base->value;
		free(ring->base);
		ring->end = ring->base = NULL;

		return val;
	}

	prev = ring->end;
	entry = ring->base;
	while(i--)
	{
		prev = entry;
		entry = entry->next;
	}
	val = entry->value;

	prev->next = entry->next;
	if(ring->base == entry)
		ring->base = entry->next;
	if(ring->end == entry)
		ring->end = prev;

	return val;
}
