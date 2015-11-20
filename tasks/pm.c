/*
 * pm.c
 *
 *  Created on: 30.12.2012
 *      Author: pascal
 */

#include "pm.h"
#include "vmm.h"
#include "memory.h"
#include "stddef.h"
#include "isr.h"
#include "stdlib.h"
#include "string.h"
#include "tss.h"
#include "cpu.h"
#include "thread.h"
#include "scheduler.h"
#include "cleaner.h"
#include "vfs.h"

static pid_t nextPID = 1;
static uint64_t numTasks = 0;
static list_t ProcessList;					//Liste aller Prozesse (Status)
extern thread_t *currentThread;
extern process_t kernel_process;				//Handler für idle-Task
extern thread_t* idleThread;				//Handler für idle-Task
thread_t* cleanerThread;				//Handler für cleaner-Task
extern list_t threadList;

ihs_t *pm_Schedule(ihs_t *cpu);

/*
 * Idle-Task
 * Wird ausgeführt, wenn kein anderer Task ausgeführt wird
 */
static void idle(void)
{
	while(1) asm volatile("hlt");
}

/*
 * Prozessverwaltung initialisieren
 */
void pm_Init()
{
	thread_Init();
	scheduler_Init();
	cleaner_Init();

	ProcessList = list_create();

	kernel_process.threads = list_create();
	idleThread = thread_create(&kernel_process, idle, 0, NULL, true);
	cleanerThread = thread_create(&kernel_process, cleaner, 0, NULL, true);

	size_t i = 0;
	thread_t *t;
	while((t = list_get(threadList, i)))
	{
		if(t == idleThread)
		{
			list_remove(threadList, i);
			break;
		}
		i++;
	}
}

/*
 * Task initialisieren
 * Parameter:	stack = Adresse zum Stack des Tasks
 * 				entry = Einsprungspunkt
 */

process_t *pm_InitTask(process_t *parent, void *entry, char* cmd, const char *stdin, const char *stdout, const char *stderr)
{
	process_t *newProcess = malloc(sizeof(process_t));

	//Argumente kopieren
	newProcess->cmd = strdup(cmd);
	if(newProcess->cmd == NULL)
	{
		free(newProcess);
		return 0;
	}

	newProcess->PID = nextPID++;
	newProcess->parent = parent;

	newProcess->Context = createContext();

	newProcess->nextThreadStack = (void*)(MM_USER_STACK + 1);

	//Liste der Threads erstellen
	newProcess->threads = list_create();

	if(!vfs_initUserspace(parent, newProcess, stdin, stdout, stderr))
	{
		//Fehler
		deleteContext(newProcess->Context);
		free(newProcess->cmd);
		free(newProcess);
		return NULL;
	}

	//Mainthread erstellen
	thread_create(newProcess, entry, strlen(newProcess->cmd) + 1, newProcess->cmd, false)->Status = READY;

	//Prozess in Liste eintragen
	list_push(ProcessList, newProcess);

	__sync_fetch_and_add(&numTasks, 1);
	newProcess->lock = LOCK_UNLOCKED;

	return newProcess;
}

/*
 * Task "zerstören", d.h. in aufräumen
 * Params:	PID = PID des Tasks
 */
void pm_DestroyTask(process_t *process)
{
	uint64_t i = 0;
	process_t *p;

	while((p = list_get(ProcessList, i++)) != NULL)
	{
		if(p == process)
		{	//Wenn der richtige Prozess gefunden wurde, alle Datenstrukturen des Prozesses freigeben
			list_remove(ProcessList, i);
			//Alle Threads beenden
			thread_t *thread;
			while((thread = list_get(process->threads, 0)))
				thread_destroy(thread);
			deleteContext(process->Context);
			free(process->cmd);
			free(process);
			numTasks--;
			break;
		}
		i++;
	}
}

/*
 * Beendet den momentan laufenden Task. Wird als Syscall aufgerufen
 * Parameter:		Registerstatus
 * Rückgabewert:	Neuer Registerstatus (Taskswitch)
 */
void pm_ExitTask(uint64_t code)
{
	cleaner_cleanProcess(currentProcess);
	yield();
}

/*
 * Hält einen Task an ohne ihn zu beenden
 * Params:	PID = PID des Tasks
 */
void pm_BlockTask(process_t *process)
{
	if(process != NULL)
	{
		process->Status = BLOCKED;

		//Alle Threads deaktivieren
		thread_t *thread;
		size_t i = 0;
		while((thread = list_get(process->threads, i++)))
		{
			scheduler_remove(thread);
		}
	}
}

/*
 * Hält einen Task an ohne ihn zu beenden
 * Params:	PID = PID des Tasks
 */
void pm_ActivateTask(process_t *process)
{
	if(process != NULL)
	{
		process->Status = READY;

		//Alle Threads aktivieren
		thread_t *thread;
		size_t i = 0;
		while((thread = list_get(process->threads, i++)))
		{
			if(thread->Status != BLOCKED)
				scheduler_add(thread);
		}
	}
}

/*
 * Gibt die Prozessstruktur mit der angebenen PID zurück
 * Parameter:	PID: PID des Tasks
 * Rückgabe:	Prozessstruktur
 */
process_t *pm_getTask(pid_t PID)
{
	uint64_t i = 0;
	process_t *Process;

	while((Process = list_get(ProcessList, i++)) != NULL)
	{
		if(Process->PID == PID)
			return Process;
	}

	return NULL;
}

}

/*
 * Scheduler. Gibt den Prozessorzustand des nächsten Tasks zurück. Der aktuelle
 * Prozessorzustand wird als Parameter übergeben und gespeichert, damit er
 * beim nächsten Aufruf des Tasks wiederhergestellt werden kann.
 */
ihs_t *pm_Schedule(ihs_t *cpu)
{
	thread_t *thread = scheduler_schedule(cpu);
	if(thread != NULL)
	{
		cpu = thread->State;
	}
	return cpu;
}
