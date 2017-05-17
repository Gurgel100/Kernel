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
#include "stdio.h"

extern void init_stdio();

void initLib()
{
	init_stdio();
}
#ifndef BUILD_KERNEL
extern int main(int argc, char *argv[]);

int getArgCount(char *cmd)
{
	int count = 0;

	int i;
	for(i = 0; ; i++)
	{
		if(cmd[i] == '\0')
		{
			count++;
			break;
		}
		else if(cmd[i] == ' ')
			count++;
	}

	return count;
}

void c_main(void *data)
{
	initLib();

	int argc = getArgCount(data);
	char *argv[argc + 1];

	argv[0] = strtok(data, " ");

	int i;
	for(i = 1; i < argc; i++)
	{
		argv[i] = strtok(NULL, " ");
	}
	argv[argc] = NULL;

	exit(main(argc, argv));
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

size_t count_envs(const char **env)
{
	size_t count = 0;

	while(env++ != NULL) count++;

	return count;
}
