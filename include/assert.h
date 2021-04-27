/*
 * assert.h
 *
 *  Created on: 21.08.2015
 *      Author: pascal
 */

#ifndef ASSERT_H_
#define ASSERT_H_

#ifdef NDEBUG
#define assert(condition) ((void)0)
#else
extern void _assert(const char *assertion, const char *file, unsigned int line, const char *function) __attribute__((noreturn));

#define assert(condition) ((condition) ? ((void)0) : _assert(#condition, __FILE__, __LINE__, ""))
#endif

#define static_assert   _Static_assert

#endif /* ASSERT_H_ */
