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
#include "pm.h"

#define VFS_SEPARATOR	'/'
#define VFS_ROOT		"/"

#define VFS_DEVICE_STORAGE		"STORAGE"
#define VFS_DEVICE_PARTITION	"PARTITION"
#define VFS_DEVICE_VIRTUAL		"VIRTUAL"

#ifndef EOF
#define EOF -1
#endif

typedef struct cdi_fs_filesystem vfs_fs_t;

typedef enum{
	TYPE_DIR, TYPE_FILE, TYPE_MOUNT, TYPE_LINK, TYPE_DEV
}vfs_node_type_t;

/*
 * FUNC_TYPE:	Gibt den Typ des Gerätes zurück
 * FUNC_NAME:	Gibt den Namen des Gerätes zurück
 * FUNC_DATA:	Gibt Gerätespezifische Daten zurück
 * 		DEVICE_PARTITION:	Gibt Dateisystem zurück
 */
typedef enum{
	FUNC_TYPE, FUNC_NAME, FUNC_DATA
}vfs_device_function_t;

//Handler für Geräte
typedef size_t (vfs_device_read_handler_t)(void *opaque, uint64_t start, size_t size, const void *buffer);
typedef size_t (vfs_device_write_handler_t)(void *opaque, uint64_t start, size_t size, const void *buffer);
typedef void *(vfs_device_getValue_handler_t)(void *opaque, vfs_device_function_t function);

typedef struct{
	//Functionen zum Lesen und Schreiben
	vfs_device_read_handler_t *read;
	vfs_device_write_handler_t *write;

	//Hiermit können verschiedene Werte ausgelesen werden
	vfs_device_getValue_handler_t *getValue;
	void *opaque;
}vfs_device_t;

typedef struct vfs_node{
		char *Name;
		vfs_node_type_t Type;
		struct vfs_node *Parent;
		struct vfs_node *Child;	//Ungültig wenn kein TYPE_DIR oder TYPE_MOUNT. Bei TYPE_LINK -> Link zum Verknüpften Element
		struct vfs_node *Next;
		union{
			vfs_device_t *dev;			//TYPE_DEVICE
			struct cdi_fs_filesystem *fs;	//TYPE_MOUNT
			size_t (*Handler)(char *name, uint64_t start, size_t length, const void *buffer);
		};
		size_t ref_count;			//Anzahl der Stream, die auf diesen Knoten verweisen
}vfs_node_t;

typedef struct{
	bool read, write, append, empty, create;
}vfs_mode_t;

typedef uint64_t vfs_file_t;

typedef struct{
	struct cdi_fs_stream stream;
	vfs_file_t id;
	vfs_mode_t mode;

	vfs_node_t *node;
}vfs_stream_t;

typedef enum{
	VFS_INFO_FILESIZE, VFS_INFO_BLOCKSIZE, VFS_INFO_USEDBLOCKS, VFS_INFO_CREATETIME, VFS_INFO_ACCESSTIME, VFS_INFO_CHANGETIME
}vfs_fileinfo_t;

void vfs_Init(void);

vfs_file_t vfs_Open(vfs_stream_t **s, const char *path, vfs_mode_t mode);
vfs_file_t vfs_Reopen(vfs_stream_t **s, const vfs_stream_t *old, vfs_mode_t mode);
void vfs_Close(vfs_stream_t *stream, vfs_file_t streamid);

/*
 * Vom Dateisystem lesen/schreiben
 * Parameter:	Path = Pfad zur Datei
 * 				Buffer = Puffer in dem die Daten reingeschrieben werden bzw. gelesen werden
 */
size_t vfs_Read(vfs_stream_t *stream, vfs_file_t streamid, uint64_t start, size_t length, const void *buffer);
size_t vfs_Write(vfs_stream_t *stream, vfs_file_t streamid, uint64_t start, size_t length, void *buffer);

/*
 * Initialisiert den Userspace des Prozesses p.
 * Parameter:	p = Prozess
 * Rückgabe:	0: kein Fehler
 * 				1: Fehler
 */
int vfs_initUserspace(process_t *parent, process_t *p, const char *stdin, const char *stdout, const char *stderr);

vfs_node_t *vfs_createNode(const char *path, const char *name, vfs_node_type_t type, void *data);

uint64_t vfs_getFileinfo(vfs_stream_t *stream, vfs_file_t streamid, vfs_fileinfo_t info);

int vfs_Mount(const char *Mountpath, const char *Dev);
int vfs_Unmount(const char *Mount);
int vfs_MountRoot(void);
int vfs_UnmountRoot(void);

/*
 * Gerät anmelden
 * Parameter:	dev = CDI-Gerät
 */
void vfs_RegisterDevice(vfs_device_t *dev);

/*
 * Gerät abmelden
 */
void vfs_UnregisterDevice(vfs_device_t *dev);

//Syscalls
vfs_file_t vfs_syscall_open(const char *path, vfs_mode_t mode);
void vfs_syscall_close(vfs_file_t streamid);
size_t vfs_syscall_read(vfs_file_t streamid, uint64_t start, size_t length, const void *buffer);
size_t vfs_syscall_write(vfs_file_t streamid, uint64_t start, size_t length, void *buffer);
uint64_t vfs_syscall_getFileinfo(vfs_file_t streamid, vfs_fileinfo_t info);

#endif /* VFS_H_ */

#endif
