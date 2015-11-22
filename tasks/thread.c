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
#include "scheduler.h"
#include "pmm.h"

list_t threadList;
tid_t nextTID = 1;

void thread_Init()
{
	threadList = list_create();
}

tid_t get_tid()
{
	static lock_t tid_lock = LOCK_UNLOCKED;
	lock(&tid_lock);
	tid_t tid = nextTID++;
	unlock(&tid_lock);
	return tid;
}

thread_t *thread_create(process_t *process, void *entry, size_t data_length, void *data, bool kernel)
{
	thread_t *thread = (thread_t*)malloc(sizeof(thread_t));
	if(thread == NULL)
		return NULL;

	thread->isMainThread = (process != currentProcess);

	thread->tid = get_tid();

	thread->process = process;

	thread->Status = BLOCKED;
	// CPU-Zustand für den neuen Task festlegen
	ihs_t new_state = {
			.cs = (kernel) ? 0x8 : 0x20 + 3,	//Kernel- oder Userspace
			.ss = (kernel) ? 0x10 : 0x18 + 3,	//Kernel- oder Userspace
			.es = 0x10,
			.ds = 0x10,
			.gs = 0x10,
			.fs = 0x10,

			.rbx = data_length,
			.rcx = cpuInfo.syscall,

			.rip = (uint64_t)entry,	//Einsprungspunkt des Programms

			.rsp = (uintptr_t)process->nextThreadStack - data_length,

			//IRQs einschalten (IF = 1)
			.rflags = 0x202,

			//Interrupt ist beim Schedulen 32
			.interrupt = 32
	};
	//Kernelstack vorbereiten
	if(!kernel)
	{
		thread->kernelStackBottom = (void*)mm_SysAlloc(1);
		thread->kernelStack = thread->kernelStackBottom + MM_BLOCK_SIZE;
		thread->State = (ihs_t*)(thread->kernelStack - sizeof(ihs_t));
		memcpy(thread->State, &new_state, sizeof(ihs_t));
	}

	thread->fpuState = NULL;

	//Stack mappen
	if(!kernel)
	{
		void *stack = (void*)mm_SysAlloc(1);
		thread->userStackBottom = process->nextThreadStack - MM_BLOCK_SIZE;
		memcpy(stack + MM_USER_STACK_SIZE - data_length, data, data_length);
		thread->userStackPhys = vmm_getPhysAddress(stack);
		if(thread->userStackPhys == 0)
			thread->userStackPhys = pmm_Alloc();
		vmm_ContextMap(process->Context, thread->userStackBottom, thread->userStackPhys,
				VMM_FLAGS_WRITE | VMM_FLAGS_USER | VMM_FLAGS_NX, 0);
		process->nextThreadStack -= MM_USER_STACK_SIZE + MM_BLOCK_SIZE;
	}
	else
	{
		new_state.rsp = (uintptr_t)mm_SysAlloc(1) + MM_BLOCK_SIZE;
		thread->State = (ihs_t*)(new_state.rsp - sizeof(ihs_t));
		memcpy(thread->State, &new_state, sizeof(ihs_t));
	}

	list_push(process->threads, thread);

	//Thread in Liste eintragen
	list_push(threadList, thread);

	thread->Status = BLOCKED;

	return thread;
}

void thread_destroy(thread_t *thread)
{
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

	//Userstack freigeben
	vmm_ContextUnMap(thread->process->Context, thread->userStackBottom);
	pmm_Free(thread->userStackPhys);

	free(thread->fpuState);
	free(thread);
}

void thread_prepare(thread_t *thread)
{
	TSS_setStack(thread->kernelStack);
}

void thread_block(thread_t *thread)
{
	if(thread != NULL && (thread->Status == RUNNING || thread->Status == READY))
	{
		scheduler_remove(thread);
		thread->Status = BLOCKED;
	}
}

void thread_unblock(thread_t *thread)
{
	if(thread != NULL && (thread->Status != RUNNING && thread->Status != READY))
	{
		thread->Status = READY;
		scheduler_add(thread);
	}
}

void thread_waitUserIO(thread_t* thread)
{
	if(thread != NULL && (thread->Status == RUNNING || thread->Status == READY))
	{
		thread->Status = WAITING_USERIO;
		scheduler_remove(thread);
		yield();
	}
}
