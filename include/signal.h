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

#define SIG_DFL __sig_dfl
#define SIG_IGN __sig_ign
#define SIG_ERR NULL

void SIG_DFL(int signal);
void SIG_IGN(int signal);
void (*signal(int sig, void (*handler)(int)))(int);
int raise(int sig);

#endif

#endif /* SIGNAL_H_ */
