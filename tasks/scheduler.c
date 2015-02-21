/*
 * scheduler.c
 *
 *  Created on: 20.02.2015
 *      Author: pascal
 */

#include "scheduler.h"
#include "list.h"
#include "lock.h"
#include "cpu.h"
#include "pm.h"
#include "stdbool.h"

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
thread_t *scheduler_schedule()
{
	static size_t actualThreadIndex = 0;
	thread_t *newThread;

	if(!active)
		return NULL;

	lock(&schedule_lock);

	if(list_size(scheduleList))		//Gibt es eigentlich etwas zu schedulen?
	{
		if(currentThread != NULL)
			currentThread->Status = READY;

		newThread = list_get(scheduleList, actualThreadIndex);
		actualThreadIndex = (actualThreadIndex + 1 < list_size(scheduleList)) ? actualThreadIndex + 1 : 0;
	}
	else
	{
		if(currentThread != NULL)
			currentThread->Status = READY;

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

	currentThread->Status = RUNNING;

	unlock(&schedule_lock);

	return currentThread;
}
