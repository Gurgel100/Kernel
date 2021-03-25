/*
 * loader.c
 *
 *  Created on: 17.10.2014
 *      Author: pascal
 */

#include "loader.h"
#include "vfs.h"
#include "elf.h"
#include "string.h"

ERROR_TYPE(pid_t) loader_load(const char *path, const char *cmd, const char **env, const char *stdin, const char *stdout, const char *stderr)
{
	char *bin;

	if(path == NULL || cmd == NULL)
		return ERROR_RETURN_ERROR(pid_t, E_INVALID_ARGUMENT);

	//Zuerst erstellen wir den Pfad zur Datei, die geöffnet werden soll
	char cmdline[strlen(cmd) + 1];
	strcpy(cmdline, cmd);
	bin = strtok(cmdline, " ");
	if(bin == NULL)
		return ERROR_RETURN_ERROR(pid_t, E_INVALID_ARGUMENT);
	char binpath[strlen(path) + 1 + strlen(cmdline) + 1];
	strcpy(binpath, path);
	binpath[strlen(binpath) + 1] = '\0';
	binpath[strlen(binpath)] = '/';
	strcat(binpath, cmdline);

	//Jetzt können wir die Datei öffnen
	vfs_stream_t *file = vfs_Open(binpath, VFS_MODE_READ);
	if(file == NULL)
		return ERROR_RETURN_ERROR(pid_t, E_IO);

	ERROR_TYPE(pid_t) elf_ret = elfLoad(file, cmd, env, stdin, stdout, stderr);
	vfs_Close(file);
	return elf_ret;
}

pid_t loader_syscall_load(const char *path, const char *cmd, const char **env, const char *stddevs[3])
{
	//TODO: check env pointers
	if(!vmm_userspacePointerValid(path, strlen(path)) || !vmm_userspacePointerValid(cmd, strlen(cmd)))
		return 0;
	if((stddevs[0] != NULL && !vmm_userspacePointerValid(stddevs[0], strlen(stddevs[0])))
		|| (stddevs[1] != NULL && !vmm_userspacePointerValid(stddevs[1], strlen(stddevs[1])))
		|| (stddevs[2] != NULL && !vmm_userspacePointerValid(stddevs[2], strlen(stddevs[2]))))
		return 0;
	return ERROR_GET_VALUE(loader_load(path, cmd, env, stddevs[0], stddevs[1], stddevs[2]));	//TODO: pass error to userspace
}
