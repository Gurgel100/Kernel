/*
 * syscall.c
 *
 *  Created on: 28.10.2014
 *      Author: pascal
 */

#ifndef BUILD_KERNEL

#include "syscall.h"

extern uint64_t (*_syscall)(uint64_t func, ...);

void *AllocPage(size_t Pages)
{
	return (void*)_syscall(0, Pages);
}

void FreePage(void *Address, size_t Pages)
{
	_syscall(1, Address, Pages);
}

void syscall_unusePage(void *Address, size_t Pages)
{
	_syscall(2, Address, Pages);
}

pid_t syscall_createProcess(const char *path, const char *cmd, const char *stdin, const char *stdout, const char *stderr)
{
	return (pid_t)_syscall(10, path, cmd, stdin, stdout, stderr);
}

void __attribute__((noreturn)) syscall_exit(int status)
{
	//Dieser syscall funktioniert nur über Interrupts
	asm volatile("int $0x30" : : "D"(11), "S"(status));
	while(1);
}

tid_t syscall_createThread(void *entry)
{
	return (tid_t)_syscall(12, entry);
}

void __attribute__((noreturn)) syscall_exitThread(int status)
{
	asm volatile("int $0x30" : : "D"(13), "S"(status));
}

void *syscall_fopen(char *path, vfs_mode_t mode)
{
	return (void*)_syscall(40, path, mode);
}

void syscall_fclose(void *stream)
{
	_syscall(41, stream);
}

size_t syscall_fread(void *stream, uint64_t start, size_t length, const void *buffer)
{
	return _syscall(42, stream, start, length, buffer);
}

size_t syscall_fwrite(void *stream, uint64_t start, size_t length, const void *buffer)
{
	return _syscall(43, stream, start, length, buffer);
}

uint64_t syscall_StreamInfo(void *stream, vfs_fileinfo_t info)
{
	return _syscall(44, stream, info);
}

void syscall_sleep(uint64_t msec)
{
	_syscall(52, msec);
}

void syscall_getSysInfo(void *Struktur)
{
	_syscall(60, Struktur);
}

#endif
