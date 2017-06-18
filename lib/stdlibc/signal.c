/*
 * signal.c
 *
 *  Created on: 05.06.2015
 *      Author: pascal
 */

#include "signal.h"
#include "stdlib.h"
#include "syscall.h"
#include "stdio.h"

#ifndef BUILD_KERNEL

static void (*signal_handlers[6])(int) = {SIG_DFL, SIG_DFL, SIG_DFL, SIG_DFL, SIG_DFL, SIG_DFL};

void SIG_DFL(int signal)
{
	switch(signal)
	{
		case SIGTERM:
			exit(EXIT_SUCCESS);
			break;
		case SIGSEGV:
			puts("segmentation fault");
			abort();
			break;
		case SIGINT:
			break;
		case SIGILL:
			puts("invalid instruction");
			abort();
		case SIGABRT:
			abort();
		case SIGFPE:
			puts("erroneous arithmetic operation");
			abort();
	}
}

void SIG_IGN(int signal __attribute__((unused)))
{
}

void (*signal(int sig, void (*handler)(int)))(int)
{
	if(sig > 5)
		return SIG_ERR;
	asm volatile("xchg %0,%1": "+r"(handler): "m"(signal_handlers[sig]): "memory");
	return handler;
}

int raise(int sig)
{
	if(sig > 5)
		return -1;
	signal_handlers[sig](sig);
	return 0;
}

#endif
