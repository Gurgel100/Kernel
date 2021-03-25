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

#include "vfs.h"
#include "pm.h"

//Funktionen
pid_t elfLoad(vfs_stream_t *file, const char *cmd, const char **env, const char *stdin, const char *stdout, const char *stderr);	//Par.: Datei = Addresse der Datei im Speicher; Segment = Segment in das kopiert werden soll (GDT)

#endif /* ELF_H_ */
