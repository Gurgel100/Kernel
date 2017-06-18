/*
 * stdint.h
 *
 *  Created on: 26.07.2012
 *      Author: pascal
 */

#ifndef STDINT_H_
#define STDINT_H_

#include <limits.h>
#include <bits/types.h>

typedef _uint128_t	uint128_t;
typedef _uint64_t	uint64_t;
typedef _uint32_t	uint32_t;
typedef _uint16_t	uint16_t;
typedef _uint8_t	uint8_t;

typedef _int128_t	int128_t;
typedef _int64_t	int64_t;
typedef _int32_t	int32_t;
typedef _int16_t	int16_t;
typedef _int8_t		int8_t;

typedef _uintptr_t	uintptr_t;
typedef _intptr_t	intptr_t;

typedef _uintmax_t		_uintmax_t;
typedef _intmax_t		_intmax_t;

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
