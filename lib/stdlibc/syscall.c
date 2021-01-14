/*
 * syscall.c
 *
 *  Created on: 28.10.2014
 *      Author: pascal
 */

#ifndef BUILD_KERNEL

#include "syscall.h"
#include <bits/syscall_numbers.h>

extern uint64_t syscall(uint64_t func, ...);

void *syscall_allocPages(size_t Pages)
{
	return (void*)syscall(SYSCALL_ALLOC_PAGES, Pages);
}

void syscall_freePages(void *Address, size_t Pages)
{
	syscall(SYSCALL_FREE_PAGES, Address, Pages);
}

void syscall_unusePages(void *Address, size_t Pages)
{
	syscall(SYSCALL_UNUSE_PAGES, Address, Pages);
}

pid_t syscall_createProcess(const char *path, const char *cmd, const char **env, const char *stdin, const char *stdout, const char *stderr)
{
	const char *stddevs[3] = {stdin, stdout, stderr};
	return (pid_t)syscall(SYSCALL_EXEC, path, cmd, env, stddevs);
}

void __attribute__((noreturn)) syscall_exit(int status)
{
	//Dieser syscall funktioniert nur über Interrupts
	syscall(SYSCALL_EXIT, status);
	while(1);
}

pid_t syscall_wait(pid_t pid, int *status)
{
	return (pid_t)syscall(SYSCALL_WAIT, pid, status);
}

tid_t syscall_createThread(void *entry, void *arg)
{
	return (tid_t)syscall(SYSCALL_THREAD_CREATE, entry, arg);
}

void __attribute__((noreturn)) syscall_exitThread(int status)
{
	syscall(SYSCALL_THREAD_EXIT, status);
}

uint64_t syscall_fopen(char *path, vfs_mode_t mode)
{
	return (void*)syscall(SYSCALL_OPEN, path, mode);
}

void syscall_fclose(uint64_t stream)
{
	syscall(SYSCALL_CLOSE, stream);
}

size_t syscall_fread(uint64_t stream, uint64_t start, size_t length, const void *buffer)
{
	return syscall(SYSCALL_READ, stream, start, length, buffer);
}

size_t syscall_fwrite(uint64_t stream, uint64_t start, size_t length, const void *buffer)
{
	return syscall(SYSCALL_WRITE, stream, start, length, buffer);
}

uint64_t syscall_getStreamInfo(uint64_t stream, vfs_fileinfo_t info)
{
	return syscall(SYSCALL_INFO_GET, stream, info);
}

void syscall_setStreamInfo(uint64_t stream, vfs_fileinfo_t info, uint64_t value)
{
	syscall(SYSCALL_INFO_SET, stream, info, value);
}

int syscall_truncate(const char *path, size_t size)
{
	return syscall(SYSCALL_TRUNCATE, path, size);
}

int syscall_mount(const char *mountpoint, const char *device)
{
	return syscall(SYSCALL_MOUNT, mountpoint, device);
}

int syscall_unmount(const char *mountpoint)
{
	return syscall(SYSCALL_UNMOUNT, mountpoint);
}

int syscall_mkdir(const char *path)
{
	return syscall(SYSCALL_MKDIR, path);
}

time_t syscall_getTimestamp()
{
	return syscall(SYSCALL_GET_TIMESTAMP);
}

void syscall_sleep(uint64_t msec)
{
	syscall(SYSCALL_SLEEP, msec);
}

void syscall_getSysInfo(SIS *Struktur)
{
	syscall(SYSCALL_SYSINF_GET, Struktur);
}

#endif
