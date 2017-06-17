/*
 * sycalls.c
 *
 *  Created on: 17.07.2012
 *      Author: pascal
 */

#ifdef BUILD_KERNEL

#include "syscalls.h"
#include "stddef.h"
#include "stdbool.h"
#include "isr.h"
#include "cmos.h"
#include "pm.h"
#include "vfs.h"
#include "loader.h"
#include "pit.h"
#include "system.h"
#include "cpu.h"
#include "scheduler.h"
#include "cleaner.h"
#include "assert.h"
#include <bits/syscall_numbers.h>

#define STAR	0xC0000081
#define LSTAR	0xC0000082
#define CSTAR	0xC0000083
#define SFMASK	0xC0000084

extern void isr_syscall();

static void nop();
static uint64_t createThreadHandler(void *entry, void *arg);
static void exitThreadHandler();
static void sleepHandler(uint64_t msec);

typedef uint64_t(*syscall)(uint64_t arg, ...);

static syscall syscalls[_SYSCALL_NUM] = {
[SYSCALL_ALLOC_PAGES]		(syscall)&mm_Alloc,
[SYSCALL_FREE_PAGES]		(syscall)&mm_Free,
[SYSCALL_UNUSE_PAGES]		(syscall)&vmm_unusePages,

[SYSCALL_EXEC]				(syscall)&loader_syscall_load,
[SYSCALL_EXIT]				(syscall)&pm_syscall_exit,
[SYSCALL_WAIT]				(syscall)&pm_syscall_wait,
[SYSCALL_THREAD_CREATE]		(syscall)&createThreadHandler,
[SYSCALL_THREAD_EXIT]		(syscall)&exitThreadHandler,

[SYSCALL_OPEN]				(syscall)&vfs_syscall_open,
[SYSCALL_CLOSE]				(syscall)&vfs_syscall_close,
[SYSCALL_READ]				(syscall)&vfs_syscall_read,
[SYSCALL_WRITE]				(syscall)&vfs_syscall_write,
[SYSCALL_INFO_GET]			(syscall)&vfs_syscall_getFileinfo,
[SYSCALL_INFO_SET]			(syscall)&vfs_syscall_setFileinfo,
[SYSCALL_MOUNT]				(syscall)&vfs_syscall_mount,
[SYSCALL_UNMOUNT]			(syscall)&vfs_syscall_unmount,

[SYSCALL_GET_TIME]			(syscall)&cmos_GetTime,
[SYSCALL_GET_DATE]			(syscall)&cmos_GetDate,
[SYSCALL_SLEEP]				(syscall)&sleepHandler,

[SYSCALL_SYSINF_GET]		(syscall)&getSystemInformation
};

void syscall_Init()
{
	//Prüfen, ob syscall/sysret unterstützt wird
	if(cpuInfo.syscall)
	{
		cpu_MSRwrite(STAR, (0x8ul << 32) | (0x13ul << 48));	//Segementregister
		cpu_MSRwrite(LSTAR, (uintptr_t)isr_syscall);		//Einsprungspunkt
		cpu_MSRwrite(SFMASK, 0);							//Wir setzen keine Bits zurück (Interrupts bleiben auch aktiviert)

		//Syscall-Instruktion aktivieren (ansonsten #UD)
		//Bit 0
		cpu_MSRwrite(0xC0000080, cpu_MSRread(0xC0000080) | 1);
	}

	for(size_t i = 0; i < _SYSCALL_NUM; i++)
	{
		if(syscalls[i] == NULL)
			syscalls[i] = (syscall)nop;
	}
}

uint64_t syscall_syscallHandler(uint64_t func, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5)
{
	//FIXME
	assert(func < _SYSCALL_NUM);
	return syscalls[func](arg1, arg2, arg3, arg4, arg5);
}

/*
 * Funktionsnummer wird im Register rdi übergeben
 */
ihs_t *syscall_Handler(ihs_t *ihs)
{
	ihs->rax = syscall_syscallHandler(ihs->rdi, ihs->rsi, ihs->rdx, ihs->rcx, ihs->r8, ihs->r9);
	return ihs;
}

static void nop()
{
	asm volatile("nop");
}

static uint64_t createThreadHandler(void *entry, void *arg)
{
	thread_t *thread = thread_create(currentProcess, entry, sizeof(void*), &arg, false);
	thread_unblock(thread);
	return thread->tid;
}

static void exitThreadHandler()
{
	cleaner_cleanThread(currentThread);
	yield();
}

static void sleepHandler(uint64_t msec)
{
	pit_RegisterTimer(currentThread, msec);
}

#endif
