/*
 * syscall.c
 *
 *  Created on: 28.10.2014
 *      Author: pascal
 */

#ifndef BUILD_KERNEL

#include "syscall.h"

inline void *AllocPage(size_t Pages)
{
	void *Address;
	asm volatile("int $0x30" :"=a"(Address) :"a"(0), "b"(Pages));
	return Address;
}

inline void FreePage(void *Address, size_t Pages)
{
	asm volatile("int $0x30" : :"a"(1), "b"(Address), "c"(Pages));
}

inline char syscall_getch()
{
	unsigned char c;
	asm volatile("int $0x30" :"=a"(c) :"a"(20));
	return c;
}

inline void syscall_putch(unsigned char c)
{
	asm volatile("int $0x30" : : "a"(21), "b"(c));
}

inline void __attribute__((noreturn)) syscall_exit(int status)
{
	asm volatile("int $0x30" : : "a"(11), "b"(status));
	while(1);
}

inline void *syscall_fopen(char *path, vfs_mode_t mode)
{
	void *ret;
	asm volatile("int $0x30" : "=a"(ret) : "a"(40), "b"(path), "c"(&mode));
	return ret;
}

inline void syscall_fclose(void *stream)
{
	asm volatile("int $0x30" : : "a"(41), "b"(stream));
}

inline size_t syscall_fread(void *stream, uint64_t start, size_t length, const void *buffer)
{
	size_t ret;
	asm volatile("int $0x30" : "=a"(ret) : "a"(42), "b"(stream), "c"(start), "d"(length), "D"(buffer));
	return ret;
}

inline size_t syscall_fwrite(void *stream, uint64_t start, size_t length, const void *buffer)
{
	size_t ret;
	asm volatile("int $0x30" : "=a"(ret) : "a"(43), "b"(stream), "c"(start), "d"(length), "D"(buffer));
	return ret;
}

inline uint64_t syscall_StreamInfo(void *stream, vfs_fileinfo_t info)
{
	uint64_t ret;
	asm volatile("int $0x30" : "=a"(ret) : "a"(44), "b"(stream), "c"(info));
	return ret;
}

#endif
