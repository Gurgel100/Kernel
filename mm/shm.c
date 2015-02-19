/*
 * shared.c
 *
 *  Created on: 18.02.2015
 *      Author: pascal
 */

#include "shm.h"
#include "list.h"

typedef struct{
	uint64_t id;
	size_t num_pages;

	list_t processes;
}shm_desc_t;

typedef struct{
	shm_desc_t *shm;
	process_t *process;
	void *ptr;
}shm_process_t;

list_t shms;
uint64_t nextID;

void shm_Init()
{
	shms = list_create();
}

/*
 * Erstellt einen neuen Shared Memory.
 *
 * Parameter:	size = Grösse des Shared Memory in Pages
 */
uint64_t shm_create(size_t size)
{
	shm_desc_t *shm = (shm_desc_t*)malloc(sizeof(shm_desc_t));
	shm->id = nextID++;
	shm->num_pages = size;
	shm->processes = list_create();

	list_push(shms, shm);

	return shm->id;
}

/*
 * Öffnet einen Shared Memory für den aktuellen Prozess
 *
 * Parameter:	id = id des Shared Memory
 * Rückgabe:	Addresse des Shared Memory
 */
void *shm_open(process_t *process, uint64_t id)
{
	shm_desc_t *shm;
	shm_process_t *shm_proc;
	size_t i = 0;
	void *ret;
	while((shm = list_get(shms, i++)))
	{
		if(shm->id == id)
			break;
	}
	if(shm == NULL)
		return NULL;

	if(list_empty(shm->processes))
	{
		ret = mm_Alloc(shm->num_pages);
	}
	else
	{
		//Ansonsten Speicher hineinmappen
	}

	shm_proc = (shm_process_t*)malloc(sizeof(shm_process_t));
	shm_proc->shm = shm;
	shm_proc->process = process;
	shm_proc->ptr = ret;
	list_push(shm->processes, shm_proc);

	return ret;
}

void shm_close(process_t *process, uint64_t id)
{
	shm_desc_t *shm;
	size_t i = 0;
	while((shm = list_get(shms, i++)))
	{
		if(shm->id == id)
			break;
	}
	if(shm == NULL)
		return;

	//SHM löschen, wenn es der letzte Prozess ist
	if(list_empty(shm->processes))
	{
		//Speicher freigeben
		vmm_Free(NULL, shm->num_pages);

		list_destroy(shm->processes);
		free(shm);
	}
	else
	{
		//Speicher unmappen
		size_t j;
		for(j = 0; j < shm->num_pages; j++)
		{
			vmm_UnMap(((shm_process_t*)list_get(shm->processes, 0))->ptr + j * 4096);
		}
	}
}
