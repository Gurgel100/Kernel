/*
 * thread.c
 *
 *  Created on: 19.02.2015
 *      Author: pascal
 */

#include "thread.h"
#include "memory.h"
#include "tss.h"
#include "vmm.h"
#include "stdlib.h"
#include "string.h"
#include "scheduler.h"
#include "pmm.h"
#include "assert.h"

static int tid_cmp(const void *a, const void *b)
{
	const thread_t *const t1 = a;
	const thread_t *const t2 = b;
	return (t1->tid < t2->tid) ? -1 : ((t1->tid == t2->tid) ? 0 : 1);
}

void thread_Init()
{
}

ERROR_TYPE_POINTER(thread_t) thread_create(process_t *process, void *entry, size_t data_length, void *data, bool kernel)
{
	if (data_length >= MM_USER_STACK_SIZE) return ERROR_RETURN_POINTER_ERROR(thread_t, E_NO_MEMORY);

	thread_t *thread = (thread_t*)malloc(sizeof(thread_t));
	if (thread == NULL) return ERROR_RETURN_POINTER_ERROR(thread_t, E_NO_MEMORY);

	thread->isMainThread = (process != currentProcess);

	thread->tid = __sync_fetch_and_add(&process->next_tid, 1);

	thread->process = process;

	thread->Status.block_reason = THREAD_BLOCKED;
	thread->Status.status = THREAD_BLOCKED_NOT_BLOCKED;
	// CPU-Zustand für den neuen Task festlegen
	ihs_t new_state = {
			.cs = (kernel) ? 0x8 : 0x20 + 3,	//Kernel- oder Userspace
			.ss = (kernel) ? 0x10 : 0x18 + 3,	//Kernel- oder Userspace
			.es = 0x10,
			.ds = 0x10,
			.gs = 0x10,
			.fs = 0x10,

			.rip = (uint64_t)entry,	//Einsprungspunkt des Programms

			.rsp = (uintptr_t)process->nextThreadStack - data_length,

			//IRQs einschalten (IF = 1)
			.rflags = 0x202,

			//Interrupt ist beim Schedulen 32
			.interrupt = 32
	};
	
	//Kernelstack vorbereiten
	assert(MM_KERN_STACK_SIZE % MM_BLOCK_SIZE == 0);
	thread->kernelStackBottom = vmm_MapGuarded(&kernel_context, NULL, 0, MM_KERN_STACK_SIZE / MM_BLOCK_SIZE, VMM_FLAGS_NX | VMM_FLAGS_GLOBAL | VMM_FLAGS_WRITE | VMM_FLAGS_ALLOCATE);
	if (thread->kernelStackBottom == NULL) {
		free(thread);
		return ERROR_RETURN_POINTER_ERROR(thread_t, E_NO_MEMORY);
	}
	thread->kernelStack = thread->kernelStackBottom + MM_KERN_STACK_SIZE;
	thread->State = (ihs_t*)(thread->kernelStack - sizeof(ihs_t));
	if (kernel) new_state.rsp = (uintptr_t)thread->kernelStackBottom + MM_KERN_STACK_SIZE;
	memcpy(thread->State, &new_state, sizeof(ihs_t));

	thread->fpuState = vmm_Map(&kernel_context, NULL, 0, (cpuInfo.xsave_area_size + MM_BLOCK_SIZE - 1) / MM_BLOCK_SIZE, VMM_FLAGS_ALLOCATE | VMM_FLAGS_NX | VMM_FLAGS_WRITE);
	thread->fpuInitialised = false;

	//Stack mappen
	if(!kernel)
	{
		assert(MM_USER_STACK_SIZE % MM_BLOCK_SIZE == 0);
		void *stack = mm_SysAlloc(MM_USER_STACK_SIZE / MM_BLOCK_SIZE);
		thread->userStackBottom = process->nextThreadStack - MM_USER_STACK_SIZE;
		memcpy(stack + MM_USER_STACK_SIZE - data_length, data, data_length);
		vmm_ReMap(&kernel_context, stack, process->Context, thread->userStackBottom, MM_USER_STACK_SIZE / MM_BLOCK_SIZE,
				VMM_FLAGS_WRITE | VMM_FLAGS_USER | VMM_FLAGS_NX);
		process->nextThreadStack -= MM_USER_STACK_SIZE + MM_BLOCK_SIZE;
	}

	avl_add(&process->threads, thread, tid_cmp);

	return ERROR_RETURN_POINTER_VALUE(thread_t, thread);
}

void thread_destroy(thread_t *thread, bool remove_from_process)
{
	vmm_UnMapGuarded(&kernel_context, thread->kernelStackBottom, MM_KERN_STACK_SIZE / MM_BLOCK_SIZE, true);

	if (remove_from_process) {
		//Thread aus Liste entfernen
		LOCKED_TASK(thread->process->lock, avl_remove(&thread->process->threads, thread, tid_cmp));
	}

	//Userstack freigeben
	vmm_UnMap(thread->process->Context, thread->userStackBottom, MM_USER_STACK_SIZE / MM_BLOCK_SIZE, true);

	extern thread_t *fpuThread;
	if(fpuThread == thread)
		fpuThread = NULL;
	vmm_UnMap(&kernel_context, thread->fpuState, 1, true);
	free(thread);
}

void thread_prepare(thread_t *thread)
{
	TSS_setStack(thread->kernelStack);
}

bool thread_block(thread_t *thread, thread_block_reason_t reason)
{
	const thread_status_t expected = {{THREAD_RUNNING, THREAD_BLOCKED_NOT_BLOCKED}};
	thread_status_t newStatus = {{THREAD_BLOCKED, reason}};
	if(__sync_bool_compare_and_swap(&thread->Status.full_status, expected.full_status, newStatus.full_status))
	{
		scheduler_remove(thread);
		return true;
	}
	return false;
}

bool thread_block_self(thread_bail_out_t bail, void *context, thread_block_reason_t reason)
{
	assert(currentThread != NULL);
	const thread_status_t expected = {{THREAD_RUNNING, THREAD_BLOCKED_NOT_BLOCKED}};
	thread_status_t newStatus = {{THREAD_BLOCKED, reason}};
	if(__sync_bool_compare_and_swap(&currentThread->Status.full_status, expected.full_status, newStatus.full_status))
	{
		scheduler_remove(currentThread);
		while(currentThread->Status.status == THREAD_BLOCKED) yield();
	}

	if(currentProcess->Status != PM_RUNNING)
	{
		if(bail != NULL)
			bail(context);
		while((currentProcess->Status != PM_RUNNING)) yield();
		return false;
	}
	return true;
}

bool thread_try_unblock(thread_t *thread)
{
	assert(thread != NULL);
	thread_status_t expected = {{THREAD_BLOCKED, thread->Status.block_reason}};
	const thread_status_t newStatus = {{THREAD_RUNNING, THREAD_BLOCKED_NOT_BLOCKED}};
	if(__sync_bool_compare_and_swap(&thread->Status.full_status, expected.full_status, newStatus.full_status))
	{
		if(!scheduler_try_add(thread))
		{
			thread->Status.block_reason = expected.block_reason;
			thread->Status.status = THREAD_BLOCKED;
			return false;
		}
		return true;
	}
	return true;
}

void thread_unblock(thread_t *thread)
{
	assert(thread != NULL);
#ifndef NDEBUG
	size_t try_count = 0;
#endif
	thread_status_t expected = {{THREAD_BLOCKED, thread->Status.block_reason}};
	const thread_status_t newStatus = {{THREAD_RUNNING, THREAD_BLOCKED_NOT_BLOCKED}};
	while(!__sync_bool_compare_and_swap(&thread->Status.full_status, expected.full_status, newStatus.full_status))
	{
		assert(++try_count > 1000 && "Thread could not been unblocked");
	}
	scheduler_add(thread);
}

void thread_waitUserIO()
{
	thread_block_self(NULL, NULL, THREAD_BLOCKED_USER_IO);
}
