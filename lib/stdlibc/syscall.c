/*
 * syscall.c
 *
 *  Created on: 28.10.2014
 *      Author: pascal
 */

#ifndef BUILD_KERNEL

#include "syscall.h"
#include <bits/syscall_numbers.h>

extern uint64_t (*_syscall)(uint64_t func, ...);

void *AllocPage(size_t Pages)
{
	return (void*)_syscall(SYSCALL_ALLOC_PAGES, Pages);
}

void FreePage(void *Address, size_t Pages)
{
	_syscall(SYSCALL_FREE_PAGES, Address, Pages);
}

void syscall_unusePage(void *Address, size_t Pages)
{
	_syscall(SYSCALL_UNUSE_PAGES, Address, Pages);
}

pid_t syscall_createProcess(const char *path, const char *cmd, const char **env, const char *stdin, const char *stdout, const char *stderr)
{
	const char *stddevs[3] = {stdin, stdout, stderr};
	return (pid_t)_syscall(SYSCALL_EXEC, path, cmd, env, stddevs);
}

void __attribute__((noreturn)) syscall_exit(int status)
{
	//Dieser syscall funktioniert nur über Interrupts
	_syscall(SYSCALL_EXIT, status);
	while(1);
}

pid_t syscall_wait(pid_t pid, int *status)
{
	return (pid_t)_syscall(SYSCALL_WAIT, pid, status);
}

tid_t syscall_createThread(void *entry, void *arg)
{
	return (tid_t)_syscall(SYSCALL_THREAD_CREATE, entry, arg);
}

void __attribute__((noreturn)) syscall_exitThread(int status)
{
	asm volatile("int $0x30" : : "D"(SYSCALL_THREAD_EXIT), "S"(status));
}

uint64_t syscall_fopen(char *path, vfs_mode_t mode)
{
	return (void*)_syscall(SYSCALL_OPEN, path, mode);
}

void syscall_fclose(uint64_t stream)
{
	_syscall(SYSCALL_CLOSE, stream);
}

size_t syscall_fread(uint64_t stream, uint64_t start, size_t length, const void *buffer)
{
	return _syscall(SYSCALL_READ, stream, start, length, buffer);
}

size_t syscall_fwrite(uint64_t stream, uint64_t start, size_t length, const void *buffer)
{
	return _syscall(SYSCALL_WRITE, stream, start, length, buffer);
}

uint64_t syscall_getStreamInfo(uint64_t stream, vfs_fileinfo_t info)
{
	return _syscall(SYSCALL_INFO_GET, stream, info);
}

void syscall_setStreamInfo(uint64_t stream, vfs_fileinfo_t info, uint64_t value)
{
	_syscall(SYSCALL_INFO_SET, stream, info, value);
}

int syscall_mount(const char *mountpoint, const char *device)
{
	return _syscall(SYSCALL_MOUNT, mountpoint, device);
}

int syscall_unmount(const char *mountpoint)
{
	return _syscall(SYSCALL_UNMOUNT, mountpoint);
}

void syscall_sleep(uint64_t msec)
{
	_syscall(SYSCALL_SLEEP, msec);
}

void syscall_getSysInfo(void *Struktur)
{
	_syscall(SYSCALL_SYSINF_GET, Struktur);
}

#endif
