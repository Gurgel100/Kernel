/*
 * ctype.c
 *
 *  Created on: 28.07.2012
 *      Author: pascal
 */

#include "ctype.h"

int isalnum(int c)
{
	return (isalpha(c) || isdigit(c));
}

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

int iscntrl(int c)
{
	return ((c >= 0x00 && c <= 0x1F) || c == 0x7F);
}

int isgraph(int c)
{
	return (isalnum(c) || ispunct(c));
}

int isprint(int c)
{
	return (isgraph(c) || c == 0x20);
}

int ispunct(int c)
{
	return ((c >= 0x21 && c <= 0x2F) || (c >= 0x3A && c <= 0x40) || (c >= 0x5B && c <= 0x60) || (c >= 0x7B && c <= 0x7E));
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
