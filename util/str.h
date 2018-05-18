/*
 * str.h
 *
 *  Created on: 20.12.2017
 *      Author: pascal
 */

#ifndef STR_H_
#define STR_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "refcount.h"

#define STRING(s)		((string_t){str = s, .len = sizeof(s) - 1, .owned = false, .isSubstr = false})
#define STRING_POS_END	((string_pos_t)-1)

typedef uintptr_t string_pos_t;

typedef struct{
	bool isSubstr: 1;
	bool owned: 1;
}string_flags_t;

typedef struct string{
	struct string *parent;
	char *str;
	size_t len;
	REFCOUNT_FIELD;
	string_flags_t flags;
}string_t;

string_t *string_createRaw(const char *str, size_t off);
string_t *string_createRawCopy(const char *str, size_t off);
void string_initRaw(string_t *string, const char *str, size_t off);
int string_initRawCopy(string_t *string, const char *str, size_t off);

string_t *string_create(string_t *str, size_t off, size_t len);
string_t *string_createCopy(string_t *str, size_t off, size_t len);
int string_init(string_t *string, string_t *str, size_t off, size_t len);
int string_initCopy(string_t *string, string_t *str, size_t off, size_t len);

void string_deinit(string_t *str);
void string_destroy(string_t *str);

size_t string_len(const string_t *str);
int string_cmp(const string_t *left, const string_t *right);
int string_cmpRaw(const string_t *left, const char *right);
int string_ncmp(const string_t *left, const string_t *right, size_t size);
string_pos_t string_chr(const string_t *str, int ch);
string_pos_t string_rchr(const string_t *str, int ch);
size_t string_spn(const string_t *str, const string_t *spn);
size_t string_spnRaw(const string_t *str, const char *spn);
size_t string_cspn(const string_t *str, const string_t *spn);
size_t string_cspnRaw(const string_t *str, const char *spn);
string_pos_t string_pbrk(const string_t *str, const string_t *breakset);
string_pos_t string_pbrkRaw(const string_t *str, const char *breakset);
string_pos_t string_str(const string_t *str, const string_t *substr);
string_pos_t string_strRaw(const string_t *str, const char *substr);

int string_cat(string_t *dest, const string_t *src);
int string_catRaw(string_t *dest, const char *src);
int string_ncat(string_t *dest, const string_t *src, size_t n);
int string_ncatRaw(string_t *dest, const char *src, size_t n);

char string_getChar(const string_t *str, size_t pos);
int string_setChar(string_t *str, size_t pos, char c);

void string_toLower(string_t *str);
void string_toUpper(string_t *str);

#endif /* STR_H_ */
