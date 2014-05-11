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
#include "cdi.h"

#define VFS_SEPARATOR	'/'
#define VFS_ROOT		"/"

typedef struct cdi_fs_filesystem vfs_fs_t;
typedef struct cdi_device vfs_dev_t;

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
void vfs_RegisterDevice(struct cdi_device *dev);

/*
 * Gerät abmelden
 */
void vfs_UnregisterDevice(struct cdi_device *dev);

#endif /* VFS_H_ */
