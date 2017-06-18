/*
 * stddef.h
 *
 *  Created on: 26.07.2012
 *      Author: pascal
 */

#ifndef STDDEF_H_
#define STDDEF_H_

#include <bits/types.h>

typedef _size_t		size_t;
typedef _ptrdiff_t	ptrdiff_t;

#ifndef NULL
#define NULL	(void*)0
#endif

#ifndef offsetof
#define offsetof(type, field) ((size_t)(&((type *)0)->field))
#endif

#endif /* STDDEF_H_ */
