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
#include "avl.h"
#include "assert.h"
#include "vfs.h"

static pid_t nextPID = 1;
static uint64_t numTasks = 0;
extern process_t kernel_process;				//Handler für idle-Task
extern thread_t* idleThread;				//Handler für idle-Task
thread_t* cleanerThread;				//Handler für cleaner-Task
extern list_t threadList;

static avl_tree *process_list = NULL;	//Liste aller Prozesse
static lock_t pm_lock = LOCK_UNLOCKED;

ihs_t *pm_Schedule(ihs_t *cpu);

/*
 * Idle-Task
 * Wird ausgeführt, wenn kein anderer Task ausgeführt wird
 */
static void idle(void)
{
	while(1) asm volatile("hlt");
}

static int pid_cmp(const void *a, const void *b, void *c)
{
	process_t *p1 = (process_t*)a;
	process_t *p2 = (process_t*)b;
	process_t **p = (process_t**)c;
	if(c != NULL && p1->PID == p2->PID)
		*p = (p1->Context == NULL) ? p2 : p1;
	return (p1->PID < p2->PID) ? -1 : ((p1->PID == p2->PID) ? 0 : 1);
}

static void pid_visit(const void *a, void *b)
{
	process_t *p = (process_t*)a;
	process_t *parent = (process_t*)((uintptr_t*)b)[0];
	list_t childs = (list_t)((uintptr_t*)b)[1];
	if(parent == p->parent)
		list_push(childs, p);
}

static void pid_list(const void *a, void *b)
{
	process_info_t *process_info = *(process_info_t**)b;
	size_t max_count = (size_t)((uintptr_t*)b)[1];
	size_t *i = (size_t*)((uintptr_t*)b)[2];
	process_t *p = (process_t*)a;

	if(*i < max_count)
	{
		process_info[*i].pid = p->PID;
		process_info[*i] = (process_info_t){
			.pid = p->PID,
			.ppid = (p->parent) ? p->parent->PID : 0,
			.num_threads = list_size(p->threads),
			.status = p->Status
		};
		memcpy(&process_info[*i].cmd, p->cmd, 19 * sizeof(char));
		process_info[*i].cmd[19] = '\0';
		(*i)++;
	}
}


/*
 * Prozessverwaltung initialisieren
 */
void pm_Init()
{
	thread_Init();
	scheduler_Init();
	cleaner_Init();

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

	newProcess->PID = __sync_fetch_and_add(&nextPID, 1);
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
	thread_create(newProcess, entry, strlen(newProcess->cmd) + 1, newProcess->cmd, false);

	//Prozess in Liste eintragen
	bool res = LOCKED_RESULT(pm_lock, avl_add_s(&process_list, newProcess, pid_cmp, NULL));
	assert(res && "Es gibt schon einen Task mit dieser PID!");

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
	if(LOCKED_RESULT(pm_lock, avl_remove_s(&process_list, process, pid_cmp, NULL)))
	{
		//Wenn der richtige Prozess gefunden wurde, alle Datenstrukturen des Prozesses freigeben
		//und alle Kindprozesse beenden
		//TODO: Signal senden anstatt einfach zu killen
		list_t childs = list_create();
		void *a[2] = {process, childs};
		LOCKED_TASK(pm_lock, avl_visit_s(process_list, avl_visiting_in_order, pid_visit, a));
		process_t *child;
		while((child = list_pop(childs)))
		{
			pm_DestroyTask(child);
		}
		list_destroy(childs);

		//Alle Threads beenden
		thread_t *thread;
		while((thread = list_get(process->threads, 0)))
			thread_destroy(thread);
		deleteContext(process->Context);
		vfs_deinitUserspace(process);
		free(process->cmd);
		free(process);
		numTasks--;
	}
	assert(!LOCKED_RESULT(pm_lock, avl_search_s(process_list, process, pid_cmp, NULL)));
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
		process->Status = PM_BLOCKED;

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
		process->Status = PM_RUNNING;

		//Alle Threads aktivieren
		thread_t *thread;
		size_t i = 0;
		while((thread = list_get(process->threads, i++)))
		{
			thread_unblock(thread);
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
	process_t *Process = NULL;
	process_t dummy = {0};
	dummy.PID = PID;

	LOCKED_TASK(pm_lock, avl_search_s(process_list, &dummy, pid_cmp, &Process));
	assert(!Process || Process->PID == PID);

	return Process;
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
