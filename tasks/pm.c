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
#include "list.h"
#include "cpu.h"

static pid_t nextPID;
static uint64_t numTasks = 0;
static list_t ProcessList;					//Liste aller Prozesse (Status)
process_t *currentProcess = NULL;			//Aktueller Prozess
static process_t *idleTask;					//Handler für idle-Task

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
	ProcessList = list_create();

	idleTask = pm_getTask(pm_InitTask(0, idle, ""));
	idleTask->State->cs = 0x8;
	idleTask->State->ds = idleTask->State->es = idleTask->State->ss = 0x10;
	idleTask->PID = 0;
	free(idleTask->cmd);
	nextPID = 1;
	//Als Stack nehmen wir den Kernelstack, weshalb wir hier den Stack wieder freigeben (1 Page)
	idleTask->State->rsp = (uint64_t)idleTask->kernelStack;
	vmm_ContextUnMap(idleTask->Context, MM_USER_STACK);

	//Jetzt den Task noch aus der Prozessliste löschen
	size_t i = 0;
	process_t *tmp;
	while((tmp = list_get(ProcessList, i)))
	{
		if(tmp == idleTask)
		{
			list_remove(ProcessList, i);
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
	newProcess->Status = BLOCKED;
	// CPU-Zustand für den neuen Task festlegen
	ihs_t new_state = {
			.cs = 0x20 + 3,	//Userspace
			.ss = 0x18 + 3,
			.es = 0x10,
			.ds = 0x10,
			.gs = 0x10,
			.fs = 0x10,

			.rcx = cpuInfo.syscall,

			.rip = (uint64_t)entry,	//Einsprungspunkt des Programms

			.rsp = MM_USER_STACK + 1,

			//IRQs einschalten (IF = 1)
			.rflags = 0x202,

			//Interrupt ist beim Schedulen 32
			.interrupt = 32
	};
	//Kernelstack vorbereiten
	newProcess->kernelStackBottom = (void*)mm_SysAlloc(1);
	newProcess->kernelStack = newProcess->kernelStackBottom + MM_BLOCK_SIZE;
	newProcess->State = (ihs_t*)(newProcess->kernelStack - sizeof(ihs_t));
	memcpy(newProcess->State, &new_state, sizeof(ihs_t));

	newProcess->Context = createContext();

	//Stack mappen (1 Page)
	vmm_ContextMap(newProcess->Context, MM_USER_STACK, 0, VMM_FLAGS_WRITE | VMM_FLAGS_USER | VMM_FLAGS_NX, VMM_UNUSED_PAGE);

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
			deleteContext(process->Context);
			mm_SysFree((uintptr_t)process->kernelStackBottom, 1);
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
	TSS_setStack(currentProcess->kernelStack);
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
