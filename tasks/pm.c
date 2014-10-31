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

typedef struct{
	process_t Process;
	void *Next;
}processlist_t;							//Prozessliste

typedef struct{
		uint64_t mmx[6];
};

static pid_t nextPID;
static uint64_t numTasks;
static processlist_t *ProcessList;		//Liste aller Prozesse (Status)
static processlist_t *lastProcess;		//letzter Prozess
static processlist_t *currentProcess;	//Aktueller Prozess

/*
 * Prozessverwaltung initialisieren
 */
void pm_Init()
{
	nextPID = 0;
	numTasks = 0;
	ProcessList = lastProcess = currentProcess = NULL;
}

/*
 * Task initialisieren
 * Parameter:	stack = Adresse zum Stack des Tasks
 * 				entry = Einsprungspunkt
 */

pid_t pm_InitTask(pid_t parent, void *entry)
{
	processlist_t *newProcess = malloc(sizeof(processlist_t));
	numTasks++;

	newProcess->Process.PID = nextPID++;
	newProcess->Process.PPID = parent;
	newProcess->Process.Active = false;
	newProcess->Process.Sleeping = false;
	// CPU-Zustand für den neuen Task festlegen
	ihs_t new_state = {
			.cs = 0x08,	//Userspace
			.ss = 0x10,
			.es = 0x10,
			.ds = 0x10,
			.gs = 0x10,
			.fs = 0x10,

			.rip = (uint64_t)entry,	//Einsprungspunkt des Programms

			.rsp = MM_USER_STACK,

			//IRQs einschalten (IF = 1)
			.rflags = 0x202,

			//Interrupt ist beim Schedulen 32
			.interrupt = 32
	};
	newProcess->Process.State = malloc(sizeof(ihs_t));
	memmove(newProcess->Process.State, &new_state, sizeof(ihs_t));
	//newProcess->State = new_state;

	newProcess->Process.Context = createContext();
	newProcess->Next = NULL;

	if(ProcessList == NULL)				//Wenn noch keine Prozessliste vorhanden ist, eine neue anlegen
		ProcessList = newProcess;
	else
		lastProcess->Next = newProcess;

	lastProcess = newProcess;

	return newProcess->Process.PID;
}

/*
 * Task "zerstören", d.h. in aufräumen
 * Params:	PID = PID des Tasks
 */
void pm_DestroyTask(pid_t PID)
{
	uint64_t i;
	processlist_t *oldProcess, *prevProcess;

	for(i = 0, oldProcess = ProcessList; i < numTasks; i++, prevProcess = oldProcess, oldProcess = ProcessList->Next)
		if(oldProcess->Process.PID == PID)
		{
			deleteContext(oldProcess->Process.Context);
			prevProcess->Next = oldProcess->Next;
			free(oldProcess->Process.State);
			free(oldProcess);
			numTasks--;
		}
}

/*
 * Hält einen Task an ohne ihn zu beenden
 * Params:	PID = PID des Tasks
 */
void pm_HaltTask(pid_t PID)
{
	uint64_t i;
	processlist_t *Process;

	for(i = 0, Process = ProcessList; i < numTasks; i++, Process = ProcessList->Next)
		if(Process->Process.PID == PID)
			Process->Process.Active = false;
}

/*
 * Hält einen Task an ohne ihn zu beenden
 * Params:	PID = PID des Tasks
 */
void pm_ActivateTask(pid_t PID)
{
	uint64_t i;
	processlist_t *Process;

	for(i = 0, Process = ProcessList; i < numTasks; i++, Process = ProcessList->Next)
		if(Process->Process.PID == PID)
			Process->Process.Active = true;
}

/*
 * Legt einen Task schlafen
 * Params:	PID = PID des Tasks
 */
void pm_SleepTask(pid_t PID)
{
	uint64_t i;
	processlist_t *Process;

	for(i = 0, Process = ProcessList; i < numTasks; i++, Process = ProcessList->Next)
		if(Process->Process.PID == PID)
			Process->Process.Sleeping = true;
}

/*
 * Weckt einen Task auf
 * Params:	PID = PID des Tasks
 */
void pm_WakeTask(pid_t PID)
{
	uint64_t i;
	processlist_t *Process;

	for(i = 0, Process = ProcessList; i < numTasks; i++, Process = ProcessList->Next)
		if(Process->Process.PID == PID)
			Process->Process.Sleeping = false;
}

/*
 * Gibt die Prozessstruktur mit der angebenen PID zurück
 * Parameter:	PID: PID des Tasks
 * Rückgabe:	Prozessstruktur
 */
process_t *pm_getTask(pid_t PID)
{
	uint64_t i;
	processlist_t *oldProcess, *prevProcess;

	for(i = 0, oldProcess = ProcessList; i < numTasks; i++, prevProcess = oldProcess, oldProcess = ProcessList->Next)
		if(oldProcess->Process.PID == PID)
			return oldProcess;

	return NULL;
}


/*
 * Scheduler. Gibt den Prozessorzustand des nächsten Tasks zurück. Der aktuelle
 * Prozessorzustand wird als Parameter übergeben und gespeichert, damit er
 * beim nächsten Aufruf des Tasks wiederhergestellt werden kann.
 */
ihs_t *pm_Schedule(ihs_t *cpu)
{

	if(currentProcess != NULL)	//Nur Zustand speichern, wenn schon in einem Task
	{
		//Wenn nur ein Task läuft, dann muss kein Taskswitch vorgenommen werden. Dies ist auch der Fall wenn nur einer aktiv ist
		if(currentProcess->Next == NULL && currentProcess == ProcessList)
			return cpu;
		memmove(currentProcess->Process.State, cpu, sizeof(ihs_t));

		do
		{
			currentProcess = (currentProcess->Next == NULL) ?
					ProcessList : currentProcess->Next;
		}
		while(currentProcess->Process.Sleeping || !currentProcess->Process.Active);

		activateContext(currentProcess->Process.Context);

		memmove(cpu, currentProcess->Process.State, sizeof(ihs_t));
	}
	else if(ProcessList != NULL)
	{
		if(ProcessList->Process.Active && !ProcessList->Process.Sleeping)
		{
			currentProcess = ProcessList;
			activateContext(currentProcess->Process.Context);
			memmove(cpu, currentProcess->Process.State, sizeof(ihs_t));
		}
	}
	return cpu;
}
