/*
 * types.h
 *
 *  Created on: 06.06.2017
 *      Author: pascal
 */

#ifndef BITS_TYPES_H_
#define BITS_TYPES_H_

typedef unsigned int __attribute__((__mode__(__TI__))) _uint128_t;
typedef unsigned long	_uint64_t;
typedef unsigned int	_uint32_t;
typedef unsigned short	_uint16_t;
typedef unsigned char	_uint8_t;

typedef signed int __attribute__((__mode__(__TI__))) _int128_t;
typedef signed long		_int64_t;
typedef signed int		_int32_t;
typedef signed short	_int16_t;
typedef signed char		_int8_t;

typedef _uint64_t		_uintptr_t;
typedef _int64_t		_intptr_t;
typedef _intptr_t 		_ptrdiff_t;

typedef _uint128_t		_uintmax_t;
typedef _int128_t		_intmax_t;

typedef unsigned long	_size_t;

typedef _uint64_t		_pid_t;
typedef _uint64_t		_tid_t;

typedef _uint64_t		_clock_t;
typedef _int64_t		_time_t;

#endif /* BITS_TYPES_H_ */
