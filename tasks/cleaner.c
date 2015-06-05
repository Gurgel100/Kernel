/*
 * cleaner.c
 *
 *  Created on: 29.05.2015
 *      Author: pascal
 */

#include "cleaner.h"
#include "list.h"
#include "lock.h"
#include "stdlib.h"
#include "scheduler.h"

extern thread_t *cleanerThread;

static list_t cleanList;
static lock_t cleanLock = LOCK_UNLOCKED;

void cleaner()
{
	while(1)
	{
		lock(&cleanLock);
		if(list_size(cleanList) > 0)
		{
			clean_entry_t *entry;
			while((entry = list_pop(cleanList)))
			{
				switch(entry->type)
				{
					case CL_PROCESS:
						pm_DestroyTask(entry->data);
					break;
					case CL_THREAD:
						thread_destroy(entry->data);
				}
				free(entry);
			}
		}
		unlock(&cleanLock);
		thread_block(cleanerThread);
		yield();
	}
}

void cleaner_Init()
{
	cleanList = list_create();
}

void cleaner_cleanProcess(process_t *process)
{
	clean_entry_t *entry = malloc(sizeof(clean_entry_t));

	entry->type = CL_PROCESS;
	entry->data = process;

	//Prozess blockieren
	pm_BlockTask(process);

	lock(&cleanLock);
	list_push(cleanList, entry);
	unlock(&cleanLock);
	thread_unblock(cleanerThread);
}

void cleaner_cleanThread(thread_t *thread)
{
	clean_entry_t *entry = malloc(sizeof(clean_entry_t));

	entry->type = CL_THREAD;
	entry->data = thread;

	//Thread blockieren
	thread_block(thread);

	lock(&cleanLock);
	list_push(cleanList, entry);
	unlock(&cleanLock);
	thread_unblock(cleanerThread);
}
