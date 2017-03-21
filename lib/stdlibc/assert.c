/*
 * assert.c
 *
 *  Created on: 21.08.2015
 *      Author: pascal
 */

#include "assert.h"
#include "stdio.h"
#ifdef BUILD_KERNEL
#include "display.h"
#else
#include "stdlib.h"
#endif

void _assert(const char *assertion, const char *file, unsigned int line, const char *function)
{
#ifdef BUILD_KERNEL
	printf("Assertion '%s' failed at line %u in %s:%s\n", assertion, line, file, function);
	Panic("KERNEL", "Assertion failure");
#else
	fprintf(stderr, "Assertion '%s' failed at line %u in %s:%s\n", assertion, line, file, function);
	abort();
#endif
}
