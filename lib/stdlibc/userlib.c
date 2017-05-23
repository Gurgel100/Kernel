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
extern char **get_environ();
extern void init_envvars(char **env);
extern int main(int argc, char *argv[]);

static int getArgCount(char *cmd)
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

void c_main(char *data)
{
	initLib();

	int argc = getArgCount(data);
	char *argv[argc + 1];

	char *env_start = (char*)((uintptr_t)data + strlen(data) + 1);
	size_t env_count = *(size_t*)env_start;
	env_start += sizeof(size_t);

	argv[0] = strtok(data, " ");

	for(int i = 1; i < argc; i++)
	{
		argv[i] = strtok(NULL, " ");
	}
	argv[argc] = NULL;

	char *envs[env_count + 1];
	for(size_t i = 0; i < env_count; i++)
	{
		envs[i] = env_start;
		env_start += strlen(env_start) + 1;
	}
	envs[env_count] = NULL;

	init_envvars(envs);

	exit(main(argc, argv));
}

pid_t createProcess(const char *path, const char *cmd, const char **env, const char *stdin, const char *stdout, const char *stderr)
{
	char **environ = env ? : get_environ();
	if(path != NULL)
	{
		return syscall_createProcess(path, cmd, environ, stdin, stdout, stderr);
	}
	else
	{
		char *env = getenv("PATH");
		if(env == NULL)
			return 0;
		char *env_copy = strdup(env);
		if(env_copy == NULL)
			return 0;

		char *env_tmp = env_copy;
		char *token;
		pid_t pid;
		while((token = strtok_s(&env_tmp, ":")) != NULL)
		{
			pid = syscall_createProcess(token, cmd, environ, stdin, stdout, stderr);
			if(pid != 0)
				break;
		}

		free(env_copy);
		return pid;
	}
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

	while(*env++ != NULL) count++;

	return count;
}
