/*
 * stddef.h
 *
 *  Created on: 26.07.2012
 *      Author: pascal
 */

#ifndef STDDEF_H_
#define STDDEF_H_

#ifndef SIZE_T
#define SIZE_T
typedef unsigned long	size_t;
#endif

typedef long ptrdiff_t;

#ifndef NULL
#define NULL	(void*)0
#endif

#ifndef offsetof
#define offsetof(type, field) ((size_t)(&((type *)0)->field))
#endif

#endif /* STDDEF_H_ */
