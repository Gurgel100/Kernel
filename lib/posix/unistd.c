/*
 * unistd.c
 *
 *  Created on: 02.09.2018
 *      Author: pascal
 */

#include "unistd.h"
#ifdef BUILD_KERNEL
#include "util.h"
#else
#include "syscall.h"
#endif

unsigned sleep(unsigned seconds)
{
#ifdef BUILD_KERNEL
	Sleep(seconds * 1000);
#else
	syscall_sleep(seconds * 1000);
#endif
	return 0;
}
