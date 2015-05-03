/*
 * syscall.c
 *
 *  Created on: 28.10.2014
 *      Author: pascal
 */

#ifndef BUILD_KERNEL

#include "syscall.h"

extern uint64_t (*_syscall)(uint64_t func, ...);

inline void *AllocPage(size_t Pages)
{
	return (void*)_syscall(0, Pages);
}

inline void FreePage(void *Address, size_t Pages)
{
	_syscall(1, Address, Pages);
}

inline void syscall_unusePage(void *Address, size_t Pages)
{
	_syscall(2, Address, Pages);
}

inline char syscall_getch()
{
	return (char)_syscall(20);
}

inline void syscall_putch(unsigned char c)
{
	_syscall(21, c);
}

inline pid_t syscall_createProcess(const char *path, const char *cmd, bool newConsole)
{
	return (pid_t)_syscall(10, path, cmd, newConsole);
}

inline void __attribute__((noreturn)) syscall_exit(int status)
{
	//Dieser syscall funktioniert nur über Interrupts
	asm volatile("int $0x30" : : "D"(11), "S"(status));
	while(1);
}

inline void *syscall_fopen(char *path, vfs_mode_t mode)
{
	return (void*)_syscall(40, path, mode);
}

inline void syscall_fclose(void *stream)
{
	_syscall(41, stream);
}

inline size_t syscall_fread(void *stream, uint64_t start, size_t length, const void *buffer)
{
	return _syscall(42, stream, start, length, buffer);
}

inline size_t syscall_fwrite(void *stream, uint64_t start, size_t length, const void *buffer)
{
	return _syscall(43, stream, start, length, buffer);
}

inline uint64_t syscall_StreamInfo(void *stream, vfs_fileinfo_t info)
{
	return _syscall(44, stream, info);
}

inline void syscall_sleep(uint64_t msec)
{
	_syscall(52, msec);
}

inline void syscall_getSysInfo(void *Struktur)
{
	_syscall(60, Struktur);
}

#endif
