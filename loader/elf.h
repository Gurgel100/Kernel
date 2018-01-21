/*
 * elf.h
 *
 *  Created on: 25.06.2012
 *      Author: pascal
 *
 *  Nach dem Tutorial "http://www.lowlevel.eu/wiki/ELF-Tutorial"
 */

#ifndef ELF_H_
#define ELF_H_

#include "stdio.h"
#include "pm.h"

//Funktionen
ERROR_TYPE(pid_t) elfLoad(FILE *fp, const char *cmd, const char **env, const char *stdin, const char *stdout, const char *stderr);	//Par.: Datei = Addresse der Datei im Speicher; Segment = Segment in das kopiert werden soll (GDT)

#endif /* ELF_H_ */
