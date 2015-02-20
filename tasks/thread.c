/*
 * thread.c
 *
 *  Created on: 19.02.2015
 *      Author: pascal
 */

#include "thread.h"
#include "list.h"
#include "lock.h"
#include "memory.h"
#include "tss.h"
#include "vmm.h"
#include "stdlib.h"
#include "string.h"
#include "cpu.h"

list_t threadList;
tid_t nextTID = 0;

void thread_Init()
{
	threadList = list_create();
}

tid_t get_tid()
{
	static lock_t tid_lock = LOCK_UNLOCKED;
	lock(&tid_lock);
	tid_t tid = nextTID;
	unlock(&tid_lock);
	return tid;
}

thread_t *thread_create(process_t *process, void *entry)
{
	thread_t *thread = (thread_t*)malloc(sizeof(thread_t));
	if(thread == NULL)
		return NULL;

	thread->isMainThread = (process != currentProcess);

	thread->tid = get_tid();

	thread->Status = BLOCKED;
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
	thread->kernelStackBottom = (void*)mm_SysAlloc(1);
	thread->kernelStack = thread->kernelStackBottom + MM_BLOCK_SIZE;
	thread->State = (ihs_t*)(thread->kernelStack - sizeof(ihs_t));
	memcpy(thread->State, &new_state, sizeof(ihs_t));

	//Stack mappen (1 Page)
	vmm_ContextMap(process->Context, MM_USER_STACK, 0, VMM_FLAGS_WRITE | VMM_FLAGS_USER | VMM_FLAGS_NX, VMM_UNUSED_PAGE);

	list_push(process->threads, thread);

	//Thread in Liste eintragen
	list_push(threadList, thread);

	thread->Status = READY;

	return thread;
}

void thread_destroy(thread_t *thread)
{
	if(thread->Status == READY)
	{
		thread->Status = BLOCKED;
	}
	else
		return;

	mm_SysFree((uintptr_t)thread->kernelStackBottom, 1);

	//Thread aus Listen entfernen
	thread_t *t;
	size_t i = 0;
	while((t = list_get(thread->process->threads, i)))
	{
		if(t == thread)
		{
			list_remove(thread->process->threads, i);
			break;
		}
		i++;
	}

	i = 0;
	while((t = list_get(threadList, i)))
	{
		if(t == thread)
		{
			list_remove(threadList, i);
			break;
		}
		i++;
	}

	free(thread);
}

void thread_prepare(thread_t *thread)
{
	TSS_setStack(thread->kernelStack);
}
