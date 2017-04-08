/*
 * cleaner.c
 *
 *  Created on: 29.05.2015
 *      Author: pascal
 */

#include "cleaner.h"
#include "stack.h"
#include "stdlib.h"
#include "scheduler.h"

extern thread_t *cleanerThread;

static stack_t *cleanStack;

void __attribute__((noreturn)) cleaner()
{
	while(1)
	{
		clean_entry_t *entry;
		while(stack_pop(cleanStack, (void**)&entry))
		{
			switch(entry->type)
			{
				case CL_PROCESS:
					pm_DestroyTask(entry->data);
				break;
				case CL_THREAD:
					while(((thread_t*)entry->data)->Status != THREAD_BLOCKED) yield();
					thread_destroy(entry->data);
			}
			free(entry);
		}
		thread_block_self(NULL, NULL);
	}
}

void cleaner_Init()
{
	cleanStack = stack_create();
}

//FIXME
void cleaner_cleanProcess(process_t *process)
{
	clean_entry_t *entry = malloc(sizeof(clean_entry_t));

	entry->type = CL_PROCESS;
	entry->data = process;

	stack_push(cleanStack, entry);
	thread_unblock(cleanerThread);
}

void cleaner_cleanThread(thread_t *thread)
{
	clean_entry_t *entry = malloc(sizeof(clean_entry_t));

	entry->type = CL_THREAD;
	entry->data = thread;

	stack_push(cleanStack, entry);
	thread_unblock(cleanerThread);
	thread_block_self(NULL, NULL);
}
