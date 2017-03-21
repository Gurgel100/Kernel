/*
 * loader.c
 *
 *  Created on: 17.10.2014
 *      Author: pascal
 */


#include "loader.h"
#include "stdio.h"
#include "elf.h"
#include "string.h"


pid_t loader_load(const char *path, const char *cmd, const char *stdin, const char *stdout, const char *stderr)
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
	strcat(binpath, cmd);

	//Jetzt können wir die Datei öffnen
	FILE *fp = fopen(binpath, "rb");
	if(fp == NULL)
		return 0;

	pid = elfLoad(fp, cmd, stdin, stdout, stderr);
	fclose(fp);
	return pid;
}
