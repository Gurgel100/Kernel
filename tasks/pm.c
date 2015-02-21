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

static pid_t nextPID;
static uint64_t numTasks = 0;
static list_t ProcessList;					//Liste aller Prozesse (Status)
process_t *currentProcess = NULL;			//Aktueller Prozess
static process_t idleProcess;				//Handler für idle-Task

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

	ProcessList = list_create();

	idleProcess.threads = list_create();
	thread_t *thread = thread_create(&idleProcess, idle);
	thread->State->cs = 0x8;
	thread->State->ss = 0x10;

	//Wir verwenden den Kernelstack weiter
	vmm_ContextUnMap(thread->process->Context, MM_USER_STACK);
	extern uint64_t stack;
	thread->State->rsp = (uint64_t)&stack;
}

/*
 * Task initialisieren
 * Parameter:	stack = Adresse zum Stack des Tasks
 * 				entry = Einsprungspunkt
 */

pid_t pm_InitTask(pid_t parent, void *entry, char* cmd)
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
	process_t *process = currentProcess;
	//Aktueller Task blockieren
	process->Status = BLOCKED;
	//Jetzt wechseln wir in den idle-Task
	currentProcess = idleTask;
	activateContext(currentProcess->Context);
	TSS_setStack(((thread_t*)list_get(currentProcess->threads, 0))->kernelStack);
	cpu = currentProcess->State;

	//Jetzt können wir den Task löschen
	pm_DestroyTask(process->PID);

	return cpu;
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
 * Scheduler. Gibt den Prozessorzustand des nächsten Tasks zurück. Der aktuelle
 * Prozessorzustand wird als Parameter übergeben und gespeichert, damit er
 * beim nächsten Aufruf des Tasks wiederhergestellt werden kann.
 */
ihs_t *pm_Schedule(ihs_t *cpu)
{
	static size_t actualProcessIndex = 0;

	if(list_size(ProcessList))		//Gibt es eigentlich etwas zu schedulen?
	{
		if(currentProcess != NULL)	//Wenn wir noch in keinem Task sind dann müssen wir auch nichts speichern
		{
			process_t *newProcess;

			if(currentProcess->Status == RUNNING)
				currentProcess->Status = READY;

			do
			{
				newProcess = list_get(ProcessList, actualProcessIndex);
				actualProcessIndex = (actualProcessIndex + 1 < list_size(ProcessList)) ? actualProcessIndex + 1 : 0;
			}
			while(newProcess->Status != READY && newProcess != currentProcess);

			//Es ist kein anderer Prozess bereit also gehen wir in den Idle-Task
			if(newProcess == currentProcess && newProcess->Status != READY)
				newProcess = idleTask;

			//Jetzt alten Prozessorzustand speichern
			currentProcess->State = cpu;

			//Hier findet der eigentliche Taskswitch statt
			currentProcess = newProcess;
			currentProcess->Status = RUNNING;
			activateContext(currentProcess->Context);
			TSS_setStack(currentProcess->kernelStack);
			cpu = currentProcess->State;
		}
		else
		{
			//Einen Task suchen, der geht
			process_t *newProcess;
			do
			{
				newProcess = list_get(ProcessList, actualProcessIndex);
				actualProcessIndex = (actualProcessIndex + 1 < list_size(ProcessList)) ? actualProcessIndex + 1 : 0;
			}
			while(newProcess->Status != READY && actualProcessIndex != 0);
			//Wenn keine erster Prozess gefunden wurde, wechseln wir auch nicht den Task
			if(newProcess == NULL || newProcess->Status != READY)
				return cpu;

			currentProcess = newProcess;

			activateContext(currentProcess->Context);
			TSS_setStack(currentProcess->kernelStack);
			cpu = currentProcess->State;
		}
	}
	return cpu;
}
