/*
 * scheduler.c
 *
 *  Created on: 20.02.2015
 *      Author: pascal
 */

#include "scheduler.h"
#include "ring.h"
#include "lock.h"
#include "stdbool.h"
#include "isr.h"

extern thread_t *fpuThread;

extern context_t kernel_context;

process_t kernel_process = {
		.Context = &kernel_context,
		.cmd = "kernel"
};
thread_t* idleThread;

static ring_t* scheduleList;
static lock_t schedule_lock = LOCK_LOCKED;
static bool active = false;
thread_t *currentThread;
process_t *currentProcess;

/*
 * Initialisiert den Scheduler
 */
void scheduler_Init()
{
	scheduleList = ring_create();
	//Lock freigeben
	unlock(&schedule_lock);
}

/*
 * Aktiviert den Scheduler
 */
void scheduler_activate()
{
	active = true;
}

/*
 * Fügt einen Thread der Schedulingliste hinzu
 *
 * Parameter:	thread = Thread, der hinzugefügt werden soll
 */
bool scheduler_try_add(thread_t *thread)
{
	if(try_lock(&schedule_lock))
	{
		ring_add(scheduleList, thread);
		unlock(&schedule_lock);
		return true;
	}
	return false;
}

/*
 * Fügt einen Thread der Schedulingliste hinzu
 *
 * Parameter:	thread = Thread, der hinzugefügt werden soll
 */
void scheduler_add(thread_t *thread)
{
	while(!scheduler_try_add(thread));
}

/*
 * Entfernt ein Thread aus der Schedulingliste
 *
 * Parameter:	thread = Thread, der entfernt werden soll
 */
void scheduler_remove(thread_t *thread)
{
	lock(&schedule_lock);
	ring_remove(scheduleList, ring_find(scheduleList, thread));
	unlock(&schedule_lock);
}

/*
 * Wechselt den aktuellen Thread
 *
 * Rückgabe:	Neuer Thread, der ausgeführt werden soll
 */
thread_t *scheduler_schedule(ihs_t *state)
{
	thread_t *newThread;

	if(!active)
		return NULL;

	if(currentThread != NULL)
	{
		currentThread->State = state;
	}

	if(try_lock((&schedule_lock)))
	{
		newThread = ring_getNext(scheduleList);
		if(newThread == NULL)
			newThread = idleThread;
		unlock(&schedule_lock);

		if(newThread != currentThread)
		{
			if(currentProcess != newThread->process)
				activateContext(newThread->process->Context);
			thread_prepare(newThread);

			currentProcess = newThread->process;
			currentThread = newThread;

			if(fpuThread == currentThread)
			{
				asm volatile("clts");
			}
			else
			{
				uint64_t cr0;
				asm volatile("mov %%cr0,%0;": "=r"(cr0));
				cr0 |= (1 << 3);							//TS-Bit setzen
				asm volatile("mov %0,%%cr0": : "r"(cr0));
			}
		}
	}

	return currentThread;
}

/*
 * Wechselt den aktuellen Thread
 */
//TODO: Interrupt für wechsel
void yield()
{
	asm volatile("int $0xFF");
}
