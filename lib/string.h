/*
 * string.h
 *
 *  Created on: 27.07.2012
 *      Author: pascal
 */

#ifndef STRINGS_H_
#define STRINGS_H_

#include "stddef.h"

#ifdef __cplusplus
extern "C" {
#endif

extern char *strcpy(char *to, const char *from);
extern int strcmp(const char *str1, const char *str2);

extern size_t strlen(const char *cs);
extern void *strdup(const char *s);
extern char *strtok(char *string, const char *delimiters);
extern char *strchr(const char *string, int c);
extern char *strrchr(const char *string, int c);

extern void *memset(void *block, int c, size_t n);
extern void *memmove(void *to, const void *from, size_t size);
extern void *memcpy (void *to, const void *from, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* STRINGS_H_ */
