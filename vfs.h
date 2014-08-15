/*
 * vfs.h
 *
 *  Created on: 13.08.2013
 *      Author: pascal
 */

#ifndef VFS_H_
#define VFS_H_

#include "cdi/fs.h"
#include "cdi.h"
#include "stdbool.h"
#include "devicemng.h"

#define VFS_SEPARATOR	'/'
#define VFS_ROOT		"/"

typedef struct cdi_fs_filesystem vfs_fs_t;

void vfs_Init(void);

/*
 * Vom Dateisystem lesen/schreiben
 * Parameter:	Path = Pfad zur Datei
 * 				Buffer = Puffer in dem die Daten reingeschrieben werden bzw. gelesen werden
 */
size_t vfs_Read(const char *Path, uint64_t start, size_t length, const void *Buffer);
size_t vfs_Write(const char *Path, uint64_t start, size_t length, const void *Buffer);

/*
 * Gerät anmelden
 * Parameter:	dev = CDI-Gerät
 */
void vfs_RegisterDevice(device_t *dev);

/*
 * Gerät abmelden
 */
void vfs_UnregisterDevice(device_t *dev);

#endif /* VFS_H_ */
