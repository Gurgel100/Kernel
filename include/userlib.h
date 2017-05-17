/*
 * userlib.h
 *
 *  Created on: 11.08.2012
 *      Author: pascal
 */

#ifndef USERLIB_H_
#define USERLIB_H_

#include "stddef.h"

#ifndef BUILD_KERNEL
#include "stdint.h"
#include "syscall.h"

extern char **get_environ();

#define createProcess(path, cmd) syscall_createProcess(path, cmd, get_environ(), NULL, NULL, NULL)
#define sleep(msec)	syscall_sleep(msec)
#define getSysInfo(Struktur) syscall_getSysInfo(Struktur)

typedef struct{
		uint64_t	physSpeicher;
		uint64_t	physFree;
		uint64_t	Uptime;
}SIS;	//"SIS" steht für "System Information Structure"
#endif

void initLib(void);

void reverse(char *s);
size_t count_envs(const char **env);

#endif /* USERLIB_H_ */
