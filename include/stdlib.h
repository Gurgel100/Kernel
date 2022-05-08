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

#define EXIT_SUCCESS	0
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

typedef struct{
		long long quot;
		long long rem;
}lldiv_t;

extern double atof(const char* str);
extern int atoi(const char *str);
extern long atol(const char *str);

extern long int strtol(const char *str, char **endptr, int base);

extern void *calloc(size_t nitems, size_t size);
extern void free(void *ptr);
extern void *malloc(size_t size);
extern void *realloc(void *ptr, size_t size);

extern void abort(void) __attribute__((noreturn));
extern int atexit(void (*func)(void));
extern void exit(int status) __attribute__((noreturn));

extern int abs(int x);
extern long labs(long x);
extern long long llabs(long long x);
extern div_t div(int numer, int denom);
extern ldiv_t ldiv(long numer, long denom);
extern lldiv_t lldiv(long long numer, long long denom);
extern int rand(void);
extern long lrand(void);
extern void srand(unsigned int seed);

extern char *getenv(const char *name);
extern int setenv(const char *name, const char *value, int overwrite);
extern int unsetenv(const char *name);
extern int putenv(char *str);

extern void qsort(void *base, size_t n, size_t size, int (*cmp)(const void*, const void*));
extern void qsort_s(void *base, size_t n, size_t size, int (*cmp)(const void*, const void*, void*), void *context);

extern void *bsearch(const void *key, const void *base, size_t n, size_t size, int (*cmp)(const void*, const void*));
extern void *bsearch_s(const void *key, const void *base, size_t n, size_t size, int (*cmp)(const void*, const void*, void*), void *context);

#ifdef __cplusplus
}
#endif

#endif /* STDLIB_H_ */
