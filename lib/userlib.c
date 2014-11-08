/*
 * userlib.c
 *
 *  Created on: 11.08.2012
 *      Author: pascal
 */

#include "userlib.h"
#include "stdlib.h"
#include "stdbool.h"
#include "string.h"

void initLib()
{
}

#ifndef BUILD_KERNEL
void getSysInfo(SIS *Struktur)
{
	asm("int $0x30" : :"a"(60), "b"(Struktur));
}
#endif

void reverse(char *s)
{
	size_t i, j;
	for(i = 0, j = strlen(s) - 1; i < j; i++, j--)
	{
		char c = s[i];
		s[i] = s[j];
		s[j] = c;
	}
}
