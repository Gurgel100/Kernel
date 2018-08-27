/*
 * system.h
 *
 *  Created on: 11.08.2012
 *      Author: pascal
 */

#ifndef SYSTEM_H_
#define SYSTEM_H_

#include <bits/sys_types.h>

extern char system_panic_buffer[25 * 80];

void getSystemInformation(SIS *Struktur);
void system_panic_enter();
void system_panic() __attribute__((noreturn));

#endif /* SYSTEM_H_ */
