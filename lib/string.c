/*
 * string.c
 *
 *  Created on: 27.07.2012
 *      Author: pascal
 */

#include "string.h"

char *strcpy(char *to, const char *from)
{
	size_t i;
	for(i = 0; from[i] != '\0'; i++)
		to[i] = from[i];
	//'\0' muss auch kopiert werden
	to[i] = from[i];
	return to;
}

int strcmp(const char *str1, const char *str2)
{
	register int i;
	for(i = 0; str1[i] != '\0' && str2[i] != '\0'; i++)
	{
		if(str1[i] == str2[i])
			continue;
		return str1[i] - str2[i];
	}
	if(str1[i] == str2[i])
		return 0;
	return str1[i] - str2[i];
}

size_t strlen(const char *cs)
{
	register size_t i;
	for(i = 0; cs[i] != '\0'; i++);
	return i;
}

void *strdup(const char *s)
{
	char *new;
	new = malloc(strlen(s) + 1);
	if(new)
		memcpy(new, s, strlen(s) + 1);
	return new;
}

char *strtok(char *string, const char *delimiters)
{
	static char *last;
	char *del, *token;
	char sc, dc;

	if(string == NULL)
	{
		if(last == NULL)
			return NULL;
		else
			string = last;
	}

	//Erste Delimiters überspringen
	cont:
	sc = *string++;
	for(del = delimiters; (dc = *del) != '\0'; del++)
		if(sc == dc)
			goto cont;

	if(sc == '\0')
	{
		last = NULL;
		return NULL;
	}
	token = string - 1;

	//String nach Delimiters durchsuchen
	while(1)
	{
		sc = *string++;
		del = delimiters;
		do
		{
			if((dc = *del++) == sc)
			{
				if(sc == '\0')
					string = NULL;
				else
					string[-1] = '\0';
				last = string;
				return token;
			}
		}
		while(dc != '\0');
	}
}

extern char *strchr(const char *string, int c)
{
	size_t i;
	size_t length = strlen(string);
	for(i = 0; i < length; i++)
	{
		if(string[i] == c)
			return string + i;
	}
	return NULL;
}

extern char *strrchr(const char *string, int c)
{
	size_t i;
	for(i = strlen(string); i > 0; i--)
	{
		if(string[i - 1] == c)
			return string + (i - 1);
	}
	return NULL;
}

void *memset(void *block, int c, size_t n)
{
	unsigned char volatile *i;
	for(i = block; i < block + n; i++)
		*i = (unsigned char)c;
	return block;
}

// TODO: Schauen, dass sich die Daten nicht überschreiben
void *memmove(void *to, const void *from, size_t size)
{
	size_t i;
	char *dest = to;
	char *src = from;
	if(from + size > to)	//Von hinten nach vorne kopieren
	{
		for(i = size; i > 0 ; i--)
		{
			dest[i - 1] = src[i - 1];
		}
	}
	else
	{
		return memcpy(to, from, size);
	}
	return to;
}

void *memcpy(void *to, const void *from, size_t size)
{
	size_t i;
	const char *src = from;
	char *dest = to;
	for(i = 0; i < size; i++)
		dest[i] = src[i];
	return to;
}
