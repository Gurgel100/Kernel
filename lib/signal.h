/*
 * signal.h
 *
 *  Created on: 05.06.2015
 *      Author: pascal
 */

#ifndef SIGNAL_H_
#define SIGNAL_H_

#ifndef BUILD_KERNEL

#define SIGTERM	0
#define SIGSEGV	1
#define SIGINT	2
#define SIGILL	3
#define SIGABRT	4
#define SIGFPE	5

#define SIG_DFL ((void (*)(int))-1)
#define SIG_IGN ((void (*)(int))-2)
#define SIG_ERR ((void (*)(int))-3)

void (*signal(int sig, void (*handler)(int)))(int);
int raise(int sig);

#endif

#endif /* SIGNAL_H_ */
