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
#include "userlib.h"

typedef struct{
	thread_t *thread;
	pid_t waiting_pid;
}pm_wait_entry_t;

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

static void pid_visit_count_childs(const void *a, void *b)
{
	const process_t *p = (const process_t*)a;
	const process_t *parent = (const process_t*)((void**)b)[0];
	if(parent == p->parent)
		(*(size_t*)(((void**)b)[1]))++;
}

static void child_terminated(process_t *parent, process_t *process)
{
	lock(&parent->lock);
	list_push(parent->terminated_childs, process);

	//wakeup any waiting thread
	pm_wait_entry_t *entry;
	thread_t *thread;
	bool found = false;
	for(size_t i = 0; (entry = list_get(parent->waiting_threads_pid, i)) != NULL; i++)
	{
		if(entry->waiting_pid == process->PID)
		{
			thread_unblock(entry->thread);
			free(entry);
			found = true;
			break;
		}
	}
	if(!found && (thread = list_pop(parent->waiting_threads)) != NULL)
	{
		thread_unblock(thread);
	}
	unlock(&parent->lock);
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

process_t *pm_InitTask(process_t *parent, void *entry, char* cmd, const char **env, const char *stdin, const char *stdout, const char *stderr)
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

	newProcess->terminated_childs = list_create();
	newProcess->waiting_threads = list_create();
	newProcess->waiting_threads_pid = list_create();

	if(!vfs_initUserspace(parent, newProcess, stdin, stdout, stderr))
	{
		//Fehler
		deleteContext(newProcess->Context);
		free(newProcess->cmd);
		free(newProcess);
		return NULL;
	}

	size_t cmd_size = strlen(newProcess->cmd) + 1;
	size_t num_envs = (env != NULL) ? count_envs(env) : 0;
	size_t env_size = 0;
	for(size_t i = 0; i < num_envs; i++)
	{
		env_size += strlen(env[i]) + 1;
	}
	void *data = malloc(cmd_size + env_size);
	strcpy(data, newProcess->cmd);
	*(size_t*)(data + cmd_size) = num_envs;
	size_t written_data = cmd_size + sizeof(size_t);
	for(size_t i = 0; i < num_envs; i++)
	{
		size_t env_len = strlen(env[i]) + 1;
		memcpy(data + written_data, env[i], env_len);
		written_data += env_len;
	}
	assert(written_data == cmd_size + sizeof(size_t) + env_size);

	//Mainthread erstellen
	thread_create(newProcess, entry, written_data, data, false);
	free(data);

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
		//free all resources of the process
		deleteContext(process->Context);
		free(process->cmd);
		free(process);
		__sync_fetch_and_add(&numTasks, -1);
	}
	assert(!LOCKED_RESULT(pm_lock, avl_search_s(process_list, process, pid_cmp, NULL)));
}

/*
 * Beendet den momentan laufenden Task. Wird als Syscall aufgerufen
 * Parameter:		Registerstatus
 * Rückgabewert:	Neuer Registerstatus (Taskswitch)
 */
void pm_ExitTask(int code)
{
	assert(currentProcess != NULL);
	assert(currentProcess->parent != NULL);

	//Alle Threads beenden
	thread_t *thread;
	while((thread = list_pop(currentProcess->threads)) != NULL)
	{
		if(thread != currentThread)
			thread_block(thread, THREAD_BLOCKED_TERMINATED);
	}

	vfs_deinitUserspace(currentProcess);

	//TODO: free userspace

	currentProcess->exit_status = code;
	currentProcess->Status = PM_TERMINATED;
	child_terminated(currentProcess->parent, currentProcess);
	thread_block_self(NULL, NULL, THREAD_BLOCKED_TERMINATED);
	assert(false);
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
			thread_block(thread, THREAD_BLOCKED_PROCESS_BLOCKED);
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

pid_t pm_WaitChild(pid_t pid, int *status)
{
	assert(currentProcess != NULL);
	process_t *child = NULL;
	if(pid == 0)
	{
		//Wait for any child
		size_t childs = 0;
		void *tmp[2] = {currentProcess, &childs};
		LOCKED_TASK(pm_lock, avl_visit_s(process_list, avl_visiting_in_order, pid_visit_count_childs, tmp));
		if(childs > 0)
		{
			lock(&currentProcess->lock);
			child = list_pop(currentProcess->terminated_childs);
			while(child == NULL)
			{
				list_push(currentProcess->waiting_threads, currentThread);
				unlock(&currentProcess->lock);
				thread_block_self(NULL, NULL, THREAD_BLOCKED_WAIT);
				lock(&currentProcess->lock);
				child = list_pop(currentProcess->terminated_childs);
			}
			unlock(&currentProcess->lock);
			if(status != NULL) *status = child->exit_status;
		}
	}
	else
	{
		//Check if pid is a child
		child = pm_getTask(pid);
		if(child != NULL && child->parent == currentProcess)
		{
			process_t *item;
			size_t i = 0;
			bool found = false;
			lock(&currentProcess->lock);
			while(!found)
			{
				while((item = list_get(currentProcess->terminated_childs, i)) != NULL)
				{
					if(item->PID == child->PID)
					{
						list_remove(currentProcess->terminated_childs, i);
						found = true;
						break;
					}
					i++;
				}
				if(!found)
				{
					pm_wait_entry_t *entry = malloc(sizeof(pm_wait_entry_t));
					entry->thread = currentThread;
					entry->waiting_pid = pid;
					list_push(currentProcess->waiting_threads_pid, entry);
					unlock(&currentProcess->lock);
					thread_block_self(NULL, NULL, THREAD_BLOCKED_WAIT);
					lock(&currentProcess->lock);
				}
			}
			unlock(&currentProcess->lock);
			if(status != NULL) *status = child->exit_status;
		}
	}

	pid_t child_pid = 0;
	if(child != NULL)
	{
		child_pid = child->PID;
		pm_DestroyTask(child);
	}

	return child_pid;
}

//syscalls
void pm_syscall_exit(int status)
{
	assert(currentThread != NULL);
	pm_ExitTask(status);
}

pid_t pm_syscall_wait(pid_t pid, int *status)
{
	assert(currentThread != NULL);
	if(status != NULL && !vmm_userspacePointerValid(status, sizeof(*status)))
		return 0;
	return pm_WaitChild(pid, status);
}
