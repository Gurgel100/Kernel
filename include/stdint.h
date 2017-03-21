/*
 * stdint.h
 *
 *  Created on: 26.07.2012
 *      Author: pascal
 */

#ifndef STDINT_H_
#define STDINT_H_

#include <limits.h>

typedef unsigned int __attribute__((__mode__(__TI__))) uint128_t;
typedef unsigned long	uint64_t;
typedef unsigned int	uint32_t;
typedef unsigned short	uint16_t;
typedef unsigned char	uint8_t;

typedef int __attribute__((__mode__(__TI__))) int128_t;
typedef signed long		int64_t;
typedef signed int		int32_t;
typedef signed short	int16_t;
typedef signed char		int8_t;
typedef uint64_t		uintptr_t;

#define INT8_MIN	SCHAR_MIN
#define INT16_MIN	SHRT_MIN
#define INT32_MIN	INT_MIN
#define INT64_MIN	LONG_MIN

#define INT8_MAX	SCHAR_MAX
#define INT16_MAX	SHRT_MAX
#define INT32_MAX	INT_MAX
#define INT64_MAX	LONG_MAX

#define UINT8_MAX	UCHAR_MAX
#define UINT16_MAX	USHRT_MAX
#define UINT32_MAX	UINT_MAX
#define UINT64_MAX	ULONG_MAX

#endif /* STDINT_H_ */
