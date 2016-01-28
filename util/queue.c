/*
 * queue.c
 *
 *  Created on: 31.08.2015
 *      Author: pascal
 */

#include "queue.h"
#include "stdlib.h"

/*
 * Erstellt eine neue Queue.
 *
 * Rückgabe:	Zeiger auf Queuestruktur
 */
queue_t *queue_create()
{
	return calloc(1, sizeof(queue_t));
}

/*
 * Zerstört eine Queue
 *
 * Parameter:	queue = Die freizugebende Queue
 */
void queue_destroy(queue_t *queue)
{
	if(queue == NULL)
		return;

	while(queue->size)
		queue_dequeue(queue);
	free(queue);
}

size_t queue_size(queue_t* queue)
{
	if(queue == NULL)
		return 0;

	return queue->size;
}

/*
 * Schiebt das Element in die Queue
 *
 * Parameter:	queue = Queue in die das nächste Element geschoben werden soll
 * 				value = Wert des Elements
 * Rückabe:		Wert des Elements
 */
void *queue_enqueue(queue_t *queue, void *value)
{
	if(queue == NULL)
		return NULL;

	queue_entry_t *entry = malloc(sizeof(queue_entry_t));
	if(entry == NULL)
		return NULL;

	entry->value = value;
	if(queue->size == 0)
		queue->begin->next = entry;
	queue->begin = entry;
	if(queue->end == NULL)
		queue->end = entry;
	queue->size++;
	return value;
}

/*
 * Holt das nächste Element aus der Queue
 *
 * Parameter:	queue = Queue aus der das nächste Element geholt werden soll
 * Rückabe:		Wert des Elements
 */
void *queue_dequeue(queue_t *queue)
{
	if(queue == NULL || queue->size == 0)
		return NULL;

	queue_entry_t *entry = queue->end;
	void *value = entry->value;
	queue->end = entry->next;
	queue->size--;
	free(entry);
	return value;
}
