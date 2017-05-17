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

#define sleep(msec)	syscall_sleep(msec)
#define getSysInfo(Struktur) syscall_getSysInfo(Struktur)

typedef struct{
		uint64_t	physSpeicher;
		uint64_t	physFree;
		uint64_t	Uptime;
}SIS;	//"SIS" steht für "System Information Structure"

pid_t createProcess(const char *path, const char *cmd, const char **env, const char *stdin, const char *stdout, const char *stderr);
#endif

void initLib(void);

void reverse(char *s);
size_t count_envs(const char **env);

#endif /* USERLIB_H_ */
