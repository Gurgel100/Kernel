/*
 * scheduler.c
 *
 *  Created on: 20.02.2015
 *      Author: pascal
 */

#include "scheduler.h"
#include "list.h"
#include "lock.h"
#include "stdbool.h"
#include "isr.h"

extern thread_t *fpuThread;

extern context_t kernel_context;

process_t idleProcess = {
		.Context = &kernel_context,
		.cmd = "idle"
};

static list_t scheduleList;
static lock_t schedule_lock = LOCK_LOCKED;
static bool active = false;
thread_t *currentThread;
process_t *currentProcess;

/*
 * Initialisiert den Scheduler
 */
void scheduler_Init()
{
	scheduleList = list_create();
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
void scheduler_add(thread_t *thread)
{
	lock(&schedule_lock);
	size_t i = list_size(scheduleList);
	list_insert(scheduleList, i ? i - 1 : 0, thread);
	unlock(&schedule_lock);
}

/*
 * Entfernt ein Thread aus der Schedulingliste
 *
 * Parameter:	thread = Thread, der entfernt werden soll
 */
void scheduler_remove(thread_t *thread)
{
	size_t i = 0;
	thread_t *thr;

	lock(&schedule_lock);
	while((thr = list_get(scheduleList, i)))
	{
		if(thr == thread)
			list_remove(scheduleList, i);
		i++;
	}
	unlock(&schedule_lock);
}

/*
 * Wechselt den aktuellen Thread
 *
 * Rückgabe:	Neuer Thread, der ausgeführt werden soll
 */
thread_t *scheduler_schedule(ihs_t *state)
{
	static size_t actualThreadIndex = 0;
	thread_t *newThread;

	if(!active)
		return NULL;

	if(currentThread != NULL)
	{
		currentThread->Status = READY;
		currentThread->State = state;
	}

	if(!locked((&schedule_lock)))
	{

		lock(&schedule_lock);

		if(list_size(scheduleList))		//Gibt es eigentlich etwas zu schedulen?
		{

			newThread = list_get(scheduleList, actualThreadIndex);
			actualThreadIndex = (actualThreadIndex + 1 < list_size(scheduleList)) ? actualThreadIndex + 1 : 0;
		}
		else
		{
			newThread = list_get(idleProcess.threads, 0);
		}

		if(newThread != currentThread)
		{
			if(currentProcess != newThread->process)
				activateContext(newThread->process->Context);
			thread_prepare(newThread);

			currentProcess = newThread->process;
			currentThread = newThread;
		}

		unlock(&schedule_lock);
	}

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

	currentThread->Status = RUNNING;

	return currentThread;
}

/*
 * Wechselt den aktuellen Thread
 */
//TODO: Interrupt für wechsel
void yield()
{
	while(currentThread->Status == BLOCKED) asm volatile("hlt");
	//asm volatile("int $0x20");
}
