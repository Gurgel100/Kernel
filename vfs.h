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

/*
 * FUNC_TYPE:	Gibt den Typ des Gerätes zurück
 * FUNC_NAME:	Gibt den Namen des Gerätes zurück
 * FUNC_DATA:	Gibt Gerätespezifische Daten zurück
 */
typedef enum{
	FUNC_TYPE, FUNC_NAME, FUNC_DATA
}vfs_device_function_t;

//Handler für Geräte
typedef size_t (vfs_device_read_handler_t)(void *opaque, uint64_t start, size_t size, void *buffer);
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

typedef struct{
	bool read, write, append, empty, create, directory;
}vfs_mode_t;

typedef uint64_t vfs_file_t;

typedef enum{
	VFS_INFO_FILESIZE, VFS_INFO_BLOCKSIZE, VFS_INFO_USEDBLOCKS, VFS_INFO_CREATETIME, VFS_INFO_ACCESSTIME, VFS_INFO_CHANGETIME
}vfs_fileinfo_t;

typedef enum{
	UDT_UNKNOWN, UDT_DIR, UDT_FILE, UDT_LINK, UDT_DEV
}vfs_userspace_direntry_type_t;

typedef struct{
	size_t size;
	vfs_userspace_direntry_type_t type;
	char name[];
}vfs_userspace_direntry_t;

void vfs_Init(void);

vfs_file_t vfs_Open(const char *path, vfs_mode_t mode);
vfs_file_t vfs_Reopen(const vfs_file_t streamid, vfs_mode_t mode);
void vfs_Close(vfs_file_t streamid);

/*
 * Vom Dateisystem lesen/schreiben
 * Parameter:	Path = Pfad zur Datei
 * 				Buffer = Puffer in dem die Daten reingeschrieben werden bzw. gelesen werden
 */
size_t vfs_Read(vfs_file_t streamid, uint64_t start, size_t length, void *buffer);
size_t vfs_Write(vfs_file_t streamid, uint64_t start, size_t length, const void *buffer);

/*
 * Initialisiert den Userspace des Prozesses p.
 * Parameter:	p = Prozess
 * Rückgabe:	0: kein Fehler
 * 				1: Fehler
 */
int vfs_initUserspace(process_t *parent, process_t *p, const char *stdin, const char *stdout, const char *stderr);

uint64_t vfs_getFileinfo(vfs_file_t streamid, vfs_fileinfo_t info);

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
size_t vfs_syscall_read(vfs_file_t streamid, uint64_t start, size_t length, void *buffer);
size_t vfs_syscall_write(vfs_file_t streamid, uint64_t start, size_t length, const void *buffer);
uint64_t vfs_syscall_getFileinfo(vfs_file_t streamid, vfs_fileinfo_t info);
int vfs_syscall_mount(const char *mountpoint, const char *device);
int vfs_syscall_unmount(const char *mountpoint);

#endif /* VFS_H_ */

#endif
