/*
 * loader.h
 *
 *  Created on: 17.10.2014
 *      Author: pascal
 */

#ifndef LOADER_H_
#define LOADER_H_

#include "pm.h"

pid_t loader_load(const char *path, const char *cmd, const char **env, const char *stdin, const char *stdout, const char *stderr);

//Syscalls
pid_t loader_syscall_load(const char *path, const char *cmd, const char **env, const char *stddevs[3]);

#endif /* LOADER_H_ */
