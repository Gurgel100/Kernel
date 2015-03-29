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

static pid_t nextPID;
static uint64_t numTasks = 0;
static list_t ProcessList;					//Liste aller Prozesse (Status)
extern thread_t *currentThread;
extern process_t idleProcess;				//Handler für idle-Task
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

	ProcessList = list_create();

	idleProcess.threads = list_create();
	thread_t *thread = thread_create(&idleProcess, idle);
	thread->State->cs = 0x8;
	thread->State->ss = 0x10;

	//Wir verwenden den Kernelstack weiter
	vmm_ContextUnMap(thread->process->Context, MM_USER_STACK);
	extern uint64_t stack;
	thread->State->rsp = (uint64_t)&stack;

	size_t i = 0;
	thread_t *t;
	while((t = list_get(threadList, i)))
	{
		if(t == thread)
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

pid_t pm_InitTask(pid_t parent, void *entry, char* cmd, bool newConsole)
{
	process_t *newProcess = malloc(sizeof(process_t));
	numTasks++;

	//Argumente kopieren
	newProcess->cmd = strdup(cmd);
	if(newProcess->cmd == NULL)
	{
		free(newProcess);
		return 0;
	}

	newProcess->PID = nextPID++;
	newProcess->PPID = parent;

	newProcess->Context = createContext();

	if(newConsole || !parent)
	{
		static uint64_t nextConsole = 1;
		char tmp[5];
		sprintf(tmp, "tty%2u", nextConsole++);
		newProcess->console = console_getByName(tmp);
	}
	else
	{
		newProcess->console = pm_getTask(parent)->console;
	}
	console_switch(newProcess->console->id);

	//Liste der Threads erstellen
	newProcess->threads = list_create();

	//Mainthread erstellen
	thread_create(newProcess, entry);

	//Prozess in Liste eintragen
	list_push(ProcessList, newProcess);

	return newProcess->PID;
}

/*
 * Task "zerstören", d.h. in aufräumen
 * Params:	PID = PID des Tasks
 */
void pm_DestroyTask(pid_t PID)
{
	uint64_t i = 0;
	process_t *process;

	while((process = list_get(ProcessList, i)) != NULL)
	{
		if(process->PID == PID)
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
ihs_t *pm_ExitTask(ihs_t *cpu, uint64_t code)
{
	thread_t *thread = currentThread;
	//Erst müssen wir den Thread wechseln
	thread_t *newThread = scheduler_schedule(cpu);

	//Jetzt müssen wir den Thread deaktivieren
	scheduler_remove(thread);
	thread->Status = BLOCKED;

	//Jetzt löschen wir den Prozess
	pm_DestroyTask(thread->process->PID);

	return newThread->State;
}

/*
 * Hält einen Task an ohne ihn zu beenden
 * Params:	PID = PID des Tasks
 */
void pm_BlockTask(pid_t PID)
{
	process_t *Process = pm_getTask(PID);

	if(Process != NULL)
	{
		Process->Status = BLOCKED;

		//Alle Threads deaktivieren
		thread_t *thread;
		size_t i = 0;
		while((thread = list_get(Process->threads, i++)))
		{
			scheduler_remove(thread);
		}
	}
}

/*
 * Hält einen Task an ohne ihn zu beenden
 * Params:	PID = PID des Tasks
 */
void pm_ActivateTask(pid_t PID)
{
	process_t *Process = pm_getTask(PID);

	if(Process != NULL)
	{
		Process->Status = READY;

		//Alle Threads aktivieren
		thread_t *thread;
		size_t i = 0;
		while((thread = list_get(Process->threads, i++)))
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

/*
 * Gibt die Konsole des aktuell ausgeführten Tasks zurück
 */
console_t *pm_getConsole()
{
	if(currentProcess)
	{
		/*if(currentProcess->console == NULL)
		{
			//Neue Konsole anlegen
			if(currentProcess->PPID > 0)
			{
				process_t *parent = pm_getTask(currentProcess->PPID);
				if(parent)
				{
					currentProcess->console = console_createChild(parent->console);
				}
				else
				{
					currentProcess->console = console_createChild(&initConsole);
				}
			}
			else
			{
				currentProcess->console = console_createChild(&initConsole);
			}
		}*/
		return currentProcess->console;
	}
	return &initConsole;
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
