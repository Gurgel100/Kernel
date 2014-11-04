/*
 * vfs.h
 *
 *  Created on: 13.08.2013
 *      Author: pascal
 */

#ifdef BUILD_KERNEL

#ifndef VFS_H_
#define VFS_H_

#include "cdi/fs.h"
#include "cdi.h"
#include "stdbool.h"
#include "devicemng.h"
#include "partition.h"

#define VFS_SEPARATOR	'/'
#define VFS_ROOT		"/"

#define EOF -1

typedef struct cdi_fs_filesystem vfs_fs_t;

typedef enum{
	TYPE_DIR, TYPE_FILE, TYPE_MOUNT, TYPE_LINK, TYPE_DEV
}vfs_node_type_t;

typedef struct vfs_node{
		char *Name;
		vfs_node_type_t Type;
		struct vfs_node *Parent;
		struct vfs_node *Child;	//Ungültig wenn kein TYPE_DIR oder TYPE_MOUNT. Bei TYPE_LINK -> Link zum Verknüpften Element
		struct vfs_node *Next;
		union{
			partition_t *partition;
			device_t *dev;
			size_t (*Handler)(char *name, uint64_t start, size_t length, const void *buffer);
		};
}vfs_node_t;

typedef struct{
	bool read, write, append, empty, create;
}vfs_mode_t;

typedef struct{
	struct cdi_fs_stream stream;
	uint64_t id;
	vfs_mode_t mode;

	vfs_node_t *node;
}vfs_stream_t;

typedef enum{
	VFS_INFO_FILESIZE, VFS_INFO_BLOCKSIZE, VFS_INFO_USEDBLOCKS, VFS_INFO_CREATETIME, VFS_INFO_ACCESSTIME, VFS_INFO_CHANGETIME
}vfs_fileinfo_t;

void vfs_Init(void);

vfs_stream_t *vfs_Open(const char *path, vfs_mode_t mode);
void vfs_Close(vfs_stream_t *stream);

/*
 * Vom Dateisystem lesen/schreiben
 * Parameter:	Path = Pfad zur Datei
 * 				Buffer = Puffer in dem die Daten reingeschrieben werden bzw. gelesen werden
 */
size_t vfs_Read(vfs_stream_t *stream, uint64_t start, size_t length, const void *buffer);
size_t vfs_Write(vfs_stream_t *stream, uint64_t start, size_t length, const void *buffer);

uint64_t vfs_getFileinfo(vfs_stream_t *stream, vfs_fileinfo_t info);

int vfs_Mount(const char *Mountpath, const char *Dev);
int vfs_Unmount(const char *Mount);
int vfs_MountRoot(void);
int vfs_UnmountRoot(void);

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

#endif
