/*
 * ctype.c
 *
 *  Created on: 28.07.2012
 *      Author: pascal
 */

#include "ctype.h"

int isalpha(int c)
{
	return (islower(c) || isupper(c));
}

int isdigit(int c)
{
	return (c >= '0' && c <= '9');
}

int isspace(int c)
{
	return (c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v');
}

int isxdigit(int c)
{
	return (isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'));
}

int islower(int c)
{
	return (c >= 'a' && c <= 'z');
}

int isupper(int c)
{
	return (c >= 'A' && c <= 'Z');
}


int tolower(int c)
{
	return (isupper(c)) ? 'a' - 'A' + c : c;
}

int toupper(int c)
{
	return (islower(c)) ? 'A' - 'a' + c : c;
}
