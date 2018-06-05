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

pid_t loader_load(const char *path, const char *cmd, const char **env, const char *stdin, const char *stdout, const char *stderr)
{
	pid_t pid = 0;
	char *bin;

	if(path == NULL || cmd == NULL)
		return 0;

	//Zuerst erstellen wir den Pfad zur Datei, die geöffnet werden soll
	char cmdline[strlen(cmd) + 1];
	strcpy(cmdline, cmd);
	bin = strtok(cmdline, " ");
	if(bin == NULL)
		return 0;
	char binpath[strlen(path) + 1 + strlen(cmdline) + 1];
	strcpy(binpath, path);
	binpath[strlen(binpath) + 1] = '\0';
	binpath[strlen(binpath)] = '/';
	strcat(binpath, cmdline);

	//Jetzt können wir die Datei öffnen
	vfs_file_t file = vfs_Open(binpath, VFS_MODE_READ);
	if(file == (vfs_file_t)-1)
		return 0;

	pid = elfLoad(file, cmd, env, stdin, stdout, stderr);
	vfs_Close(file);
	return pid;
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
	return loader_load(path, cmd, env, stddevs[0], stddevs[1], stddevs[2]);
}
