/*
 * stdarg.h
 *
 *  Created on: 22.10.2012
 *      Author: pascal
 */

#ifndef STDARG_H_
#define STDARG_H_

#ifdef __cplusplus
extern "C" {
#endif

	typedef __builtin_va_list	va_list;
	#define va_start(ap, X)		__builtin_va_start(ap, X)
	#define va_arg(ap, type)	__builtin_va_arg(ap, type)
	#define va_end(ap)			__builtin_va_end(ap)
	#define va_copy(dest, src)	__builtin_va_copy(dest, src)

#ifdef __cplusplus
}
#endif

#endif /* STDARG_H_ */
