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
	while(1) asm volatile("hlt");
}

#endif
