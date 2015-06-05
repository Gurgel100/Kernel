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

static void signal_handler(int signal);

static void (*signal_handlers[6])(int) = {signal_handler, signal_handler, signal_handler, signal_handler, signal_handler, signal_handler};

static void signal_handler(int signal)
{
	switch(signal)
	{
		case SIGTERM:
			exit(EXIT_SUCCES);
			break;
		case SIGSEGV:
			puts("segmention fault");
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

void (*signal(int sig, void (*handler)(int)))(int)
{
	if(sig > 5)
		return SIG_ERR;
	void (*old_handler)(int) = signal_handlers[sig];
	if(handler == SIG_DFL)
		signal_handlers[sig] = signal_handler;
	else if(handler == SIG_IGN)
		signal_handlers[sig] = NULL;
	else
		signal_handlers[sig] = handler;
	return old_handler;
}

int raise(int sig)
{
	if(sig > 5)
		return -1;
	if(signal_handlers[sig] != NULL)
	{
		printf("raise signal %i\n", sig);
		signal_handlers[sig](sig);
	}
	return 0;
}

#endif
