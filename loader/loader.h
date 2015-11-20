/*
 * loader.h
 *
 *  Created on: 17.10.2014
 *      Author: pascal
 */

#ifndef LOADER_H_
#define LOADER_H_

#include "pm.h"

pid_t loader_load(const char *path, const char *cmd, const char *stdin, const char *stdout, const char *stderr);

#endif /* LOADER_H_ */
