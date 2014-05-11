/*
 * stdint.h
 *
 *  Created on: 26.07.2012
 *      Author: pascal
 */

#ifndef STDINT_H_
#define STDINT_H_

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

#endif /* STDINT_H_ */
