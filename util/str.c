/*
 * str.c
 *
 *  Created on: 20.12.2017
 *      Author: pascal
 */

#include "str.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#define MIN(a, b)	((a < b) ? a : b)
#define MAX(a, b)	((a > b) ? a : b)

static string_t *getRoot(string_t *str)
{
	while(str->flags.isSubstr) str = str->parent;
	return str;
}

static void init(string_t *string, const char *str, size_t len, size_t off)
{
	string->str = (char*)str + off;
	string->len = len - off;
	string->flags.owned = false;
	string->flags.isSubstr = false;
}

static int initCopy(string_t *string, const char *str, size_t len, size_t off)
{
	string->str = malloc(string->len);
	if(string->str == NULL)
		return 1;

	string->len = len - off;
	string->flags.owned = true;
	string->flags.isSubstr = false;

	memcpy(string->str, str + off, string->len);
	return 0;
}

static int initSubstr(string_t *string, string_t *parent, size_t off, size_t len)
{
	if(off > parent->len)
		return 1;

	string_t *root = getRoot(parent);
	if(REFCOUNT_RETAIN(root) == NULL)
		return 1;
	string->str = parent->str + off;
	string->len = MIN(len, parent->len - off);
	string->parent = root;
	string->flags.isSubstr = true;

	assert(string->len + off > parent->len && "End of substring out of bounds of parent string");
	assert(string->str >= root->str && string->str + string->len <= root->str + root->len);

	return 0;
}

string_t *string_createRaw(const char *str, size_t off)
{
	string_t *string = malloc(sizeof(string_t));
	if(string == NULL)
		return NULL;
	init(string, str, strlen(str), off);
	REFCOUNT_INIT(string, string_destroy);
	return string;
}

string_t *string_createRawCopy(const char *str, size_t off)
{
	string_t *string = malloc(sizeof(string_t));
	if(string == NULL)
		return NULL;
	if(initCopy(string, str, strlen(str), off))
	{
		free(string);
		return NULL;
	}
	REFCOUNT_INIT(string, string_destroy);
	return string;
}

void string_initRaw(string_t *string, const char *str, size_t off)
{
	init(string, str, strlen(str), off);
	REFCOUNT_INIT(string, string_deinit);
}

int string_initRawCopy(string_t *string, const char *str, size_t off)
{
	REFCOUNT_INIT(string, string_deinit);
	return initCopy(string, str, strlen(str), off);
}

string_t *string_create(string_t *str, size_t off, size_t len)
{
	string_t *string = malloc(sizeof(string_t));
	if(string == NULL)
		return NULL;
	if(initSubstr(string, str, off, MIN(str->len, len)))
	{
		free(string);
		return NULL;
	}
	REFCOUNT_INIT(string, string_destroy);
	return string;
}

string_t *string_createCopy(string_t *str, size_t off, size_t len)
{
	string_t *string = malloc(sizeof(string_t));
	if(string == NULL)
		return NULL;
	initCopy(string, str->str, MIN(str->len, len), off);
	REFCOUNT_INIT(string, string_destroy);
	return string;
}

int string_init(string_t *string, string_t *str, size_t off, size_t len)
{
	REFCOUNT_INIT(string, string_deinit);
	return initSubstr(string, str, off, MIN(str->len, len));
}

int string_initCopy(string_t *string, string_t *str, size_t off, size_t len)
{
	REFCOUNT_INIT(string, string_deinit);
	return initCopy(string, str->str, MIN(str->len, len), off);
}

void string_deinit(string_t *str)
{
	if(str->flags.isSubstr)
	{
		REFCOUNT_RELEASE(str->parent);
	}
	else if(str->flags.owned)
	{
		free(str->str);
	}
}

void string_destroy(string_t *str)
{
	string_deinit(str);
	free(str);
}

size_t string_len(const string_t *str)
{
	return str->len;
}

int string_cmp(const string_t *left, const string_t *right)
{
	if(left->len > right->len)
		return left->str[right->len + 1];

	if(left->len < right->len)
		return -right->str[left->len + 1];

	return memcmp(left->str, right->str, left->len);
}

int string_cmpRaw(const string_t *left, const char *right)
{
	for(size_t i = 0; i < left->len; i++)
	{
		if(right[i] == '\0' || left->str[i] != right[i])
			return left->str[i] - right[i];
	}
	return -right[left->len];
}

int string_ncmp(const string_t *left, const string_t *right, size_t size)
{
	return 0;
}

string_pos_t string_chr(const string_t *str, int ch)
{
	const char *match = memchr(str->str, ch, str->len);
	if(match == NULL)
		return STRING_POS_END;
	else
		return match - str->str;
}

string_pos_t string_rchr(const string_t *str, int ch)
{
	const char *p = str->str + str->len;
	while(p > str->str && *p != ch) p--;

	if(p > str->str || *p == ch)
		return p - str->str;
	else
		return STRING_POS_END;
}

static bool spanContains(const char *spn, size_t len, int ch)
{
	for(size_t j = 0; j < len; j++)
	{
		if(ch == spn[j])
			return true;
	}
	return false;
}

static size_t _spn(const string_t *str, const char *spn, size_t spn_len)
{
	for(size_t i = 0; i < str->len; i++)
	{
		if(!spanContains(spn, spn_len, str->str[i]))
			return i;
	}

	return str->len;
}

static size_t _cspn(const string_t *str, const char *spn, size_t spn_len)
{
	for(size_t i = 0; i < str->len; i++)
	{
		if(spanContains(spn, spn_len, str->str[i]))
			return i;
	}

	return str->len;
}

size_t string_spn(const string_t *str, const string_t *spn)
{
	return _spn(str, spn->str, spn->len);
}

size_t string_spnRaw(const string_t *str, const char *spn)
{
	return _spn(str, spn, strlen(spn));
}

size_t string_cspn(const string_t *str, const string_t *spn)
{

	return _cspn(str, spn->str, spn->len);
}

size_t string_cspnRaw(const string_t *str, const char *spn)
{
	return _cspn(str, spn, strlen(spn));
}

static string_pos_t _str(const string_t *str, const char *substr, size_t substr_len)
{
	if(str->len > substr_len)
	{
		for(string_pos_t i = 0; i < str->len - substr_len + (substr_len > 0); i++)
		{
			if(memcmp(&str->str[i], substr, substr_len) == 0)
				return i;
		}
	}
	else if(str->len == substr_len && string_cmpRaw(str, substr) == 0)
	{
		return 0;
	}
	return STRING_POS_END;
}

string_pos_t string_str(const string_t *str, const string_t *substr)
{
	return _str(str, substr->str, substr->len);
}

string_pos_t string_strRaw(const string_t *str, const char *substr)
{
	return _str(str, substr, strlen(substr));
}

static int _cat(string_t *dest, const char *src, size_t src_len)
{
	char *new_str;

	if(dest->flags.isSubstr || !dest->flags.owned)
		new_str = malloc(dest->len + src_len);
	else
		new_str = realloc(dest->str, dest->len + src_len);

	if(new_str == NULL)
		return 1;

	const char *old_str = dest->str;
	size_t old_len = dest->len;

	dest->str = new_str;
	dest->len += src_len;

	if(dest->flags.isSubstr)
		REFCOUNT_RELEASE(dest->parent);

	dest->flags.isSubstr = false;
	dest->flags.owned = true;

	memcpy(dest->str, old_str, old_len);
	memcpy(dest->str + old_len, src, src_len);

	return 0;
}

int string_cat(string_t *dest, const string_t *src)
{
	return _cat(dest, src->str, src->len);
}

int string_catRaw(string_t *dest, const char *src)
{
	return _cat(dest, src, strlen(src));
}

int string_ncat(string_t *dest, const string_t *src, size_t n)
{
	return _cat(dest, src->str, MIN(src->len, n));
}

int string_ncatRaw(string_t *dest, const char *src, size_t n)
{
	size_t src_len = strlen(src);
	return _cat(dest, src, MIN(src_len, n));
}

char string_getChar(const string_t *str, size_t pos)
{
	return str->str[pos];
}

int string_setChar(string_t *str, size_t pos, char c)
{
	if(pos > str->len || str->flags.isSubstr || !str->flags.owned)
	{
		char *new_str;
		size_t new_len = MAX(pos + 1, str->len);
		if(str->flags.isSubstr || !str->flags.owned)
		{
			new_str = malloc(new_len);
			if(new_str == NULL)
				return 1;

			memcpy(new_str, str->str, str->len);
			memset(str->str + str->len, 0, new_len - str->len);
		}
		else
		{
			new_str = realloc(str->str, new_len);
			if(new_str == NULL)
				return 1;
		}

		str->len = new_len;
		str->str = new_str;

		if(str->flags.isSubstr)
			REFCOUNT_RELEASE(str->parent);

		str->flags.isSubstr = false;
		str->flags.owned = true;
	}
	str->str[pos] = c;
	return 0;
}

void string_toLower(string_t *str)
{
	for(size_t i = 0; i < str->len; i++)
	{
		if(str->str[i] >= 'A' && str->str[i] <= 'Z')
			str->str[i] = str->str[i] - 'A' + 'a';
	}
}

void string_toUpper(string_t *str)
{
	for(size_t i = 0; i < str->len; i++)
	{
		if(str->str[i] >= 'a' && str->str[i] <= 'z')
			str->str[i] = str->str[i] - 'a' + 'A';
	}
}
