/*
 * stdlib.h
 *
 *  Created on: 26.07.2012
 *      Author: pascal
 */

#ifndef STDLIB_H_
#define STDLIB_H_

#include "stddef.h"
#include "limits.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EXIT_SUCCES		0
#define EXIT_FAILURE	1
#define RAND_MAX		INT_MAX

typedef struct{
		int quot;
		int rem;
}div_t;

typedef struct{
		long quot;
		long rem;
}ldiv_t;

extern long int strtol(const char *str, char **endptr, int base);

extern void *calloc(size_t nitems, size_t size);
extern void free(void *ptr);
extern void *malloc(size_t size);
extern void *realloc(void *ptr, size_t size);

extern void abort(void);
extern int atexit(void (*func)(void));
extern void exit(int status);

extern int abs(int x);
extern long int labs(long int x);
extern div_t div(int numer, int denom);
extern ldiv_t ldiv(long int numer, long int denom);
extern int rand(void);
extern long lrand(void);
extern void srand(unsigned int seed);

#ifdef __cplusplus
}
#endif

#endif /* STDLIB_H_ */
