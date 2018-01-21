/*
 * setjmp.h
 *
 *  Created on: 16.10.2017
 *      Author: pascal
 */

#ifndef SETJMP_H_
#define SETJMP_H_

#include <bits/types.h>

#define setjmp(env)	setjmp(env)

typedef _uintptr_t jmp_buf[8];

int setjmp(jmp_buf env);
void longjmp(jmp_buf env, int status) __attribute__((noreturn));

#endif /* SETJMP_H_ */
