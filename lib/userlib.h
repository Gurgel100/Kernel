/*
 * userlib.h
 *
 *  Created on: 11.08.2012
 *      Author: pascal
 */

#ifndef USERLIB_H_
#define USERLIB_H_

#ifndef BUILD_KERNEL
#include "stdint.h"
#include "syscall.h"

#define createProcess(path, cmd) syscall_createProcess(path, cmd)
#define sleep(msec)	syscall_sleep(msec)

typedef struct{
		uint64_t	physSpeicher;
		uint64_t	physFree;
		uint64_t	Uptime;
}SIS;	//"SIS" steht für "System Information Structure"
#endif

void initLib(void);

#ifndef BUILD_KERNEL
void getSysInfo(SIS *Struktur);
#endif

void reverse(char *s);

#endif /* USERLIB_H_ */
