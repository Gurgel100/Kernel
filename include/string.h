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
extern char *strncpy(char *to, const char *from, size_t size);
extern int strcmp(const char *str1, const char *str2);
extern int strncmp(const char *str1, const char *str2, size_t size);

extern size_t strlen(const char *cs);
extern void *strdup(const char *s);
extern char *strtok(char *string, const char *delimiters);
extern char *strtok_s(char **str, const char *delimiters);
extern char *strcat(char *str1, const char *str2);
extern char *strncat(char *str1, const char *str2, size_t n);

extern char *strchr(const char *str, int ch);
extern char *strrchr(const char *str, int ch);
extern size_t strspn(const char *dest, const char *src);
extern size_t strcspn(const char *dest, const char *src);
extern char *strpbrk(const char *dest, const char *breakset);
extern char *strstr(const char *str, const char *substr);

extern void *memchr(const void *ptr, int ch, size_t count);
extern int memcmp(const void *lhs, const void *rhs, size_t count);
extern void *memset(void *block, int c, size_t n);
extern void *memmove(void *to, const void *from, size_t size);
extern void *memcpy (void *to, const void *from, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* STRINGS_H_ */
