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
void vfs_Read(const char *Path, const void *Buffer);
void vfs_Write(const char *Path, const void *Buffer);

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
