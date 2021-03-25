/*
 * vfs.h
 *
 *  Created on: 13.08.2013
 *      Author: pascal
 */

/**
 * @file
 * @brief virtual filesystem
 */

#ifndef VFS_H_
#define VFS_H_

#include "stdbool.h"
#include "pm.h"
#include <bits/sys_types.h>
#include "node.h"
#include <refcount.h>
#include <bits/error.h>

#define VFS_SEPARATOR	'/'
#define VFS_ROOT		"/"

#ifndef EOF
#define EOF -1
#endif

typedef uint64_t vfs_file_t;
typedef struct vfs_stream vfs_stream_t;

typedef enum{
	VFS_FILESYSTEM_INFO_VIRTUAL	= 1 << 0,
	VFS_FILESYSTEM_INFO_MODULE	= 1 << 1,
}vfs_filesystem_info_t;

typedef struct vfs_filesystem vfs_filesystem_t;

typedef struct{
	/**
	 * Name of the filesystem driver
	 */
	const char *name;
	vfs_filesystem_info_t info;

	int (*probe)(vfs_filesystem_t *fs, vfs_stream_t *dev);
	int (*mount)(vfs_filesystem_t *fs, vfs_stream_t *dev);
	int (*umount)(vfs_filesystem_t *fs);
	/**
	 * If VFS_FILESYSTEM_INFO_MODULE is set
	 */
	vfs_node_dir_t *(*get_root)();
}vfs_filesystem_driver_t;

struct vfs_filesystem{
	/**
	 * Reference counting field
	 */
	REFCOUNT_FIELD;

	/**
	 * The filesystem driver which is in charge
	 */
	vfs_filesystem_driver_t *driver;

	/**
	 * The root node. This field is only valid once the filesystem is mounted.
	 */
	vfs_node_dir_t *root;

	/**
	 * This field can be used by the driver
	 */
	void *opaque;
};

ERROR_TYPEDEF_POINTER(vfs_stream_t);

void vfs_Init(void);

ERROR_TYPE_POINTER(vfs_stream_t) vfs_Open(const char *path, vfs_mode_t mode);
ERROR_TYPE_POINTER(vfs_stream_t) vfs_Reopen(vfs_stream_t *stream, vfs_mode_t mode);
void vfs_Close(vfs_stream_t *stream);

/*
 * Vom Dateisystem lesen/schreiben
 * Parameter:	Path = Pfad zur Datei
 * 				Buffer = Puffer in dem die Daten reingeschrieben werden bzw. gelesen werden
 */
size_t vfs_Read(vfs_stream_t *stream, uint64_t start, size_t length, void *buffer);
size_t vfs_Write(vfs_stream_t *stream, uint64_t start, size_t length, const void *buffer);

/*
 * Initialisiert den Userspace des Prozesses p.
 * Parameter:	p = Prozess
 * Rückgabe:	0: kein Fehler
 * 				1: Fehler
 */
int vfs_initUserspace(process_t *parent, process_t *p, const char *stdin, const char *stdout, const char *stderr);
void vfs_deinitUserspace(process_t *p);

uint64_t vfs_getFileinfo(vfs_stream_t *stream, vfs_fileinfo_t info);

int vfs_truncate(const char *path, size_t size);

int vfs_createDir(const char *path);

int vfs_Mount(const char *mountpath, const char *devpath, const char *filesystem);
int vfs_Unmount(const char *mountpath);
int vfs_MountRoot(void);

int vfs_registerFilesystemDriver(vfs_filesystem_driver_t *driver);

//Syscalls
vfs_file_t vfs_syscall_open(const char *path, vfs_mode_t mode);
void vfs_syscall_close(vfs_file_t streamid);
size_t vfs_syscall_read(vfs_file_t streamid, uint64_t start, size_t length, void *buffer);
size_t vfs_syscall_write(vfs_file_t streamid, uint64_t start, size_t length, const void *buffer);
uint64_t vfs_syscall_getFileinfo(vfs_file_t streamid, vfs_fileinfo_t info);
void vfs_syscall_setFileinfo(vfs_file_t streamid, vfs_fileinfo_t info, uint64_t value);
int vfs_syscall_truncate(const char *path, size_t size);
int vfs_syscall_mkdir(const char *path);
int vfs_syscall_mount(const char *mountpoint, const char *device, const char *filesystem);
int vfs_syscall_unmount(const char *mountpoint);

#endif /* VFS_H_ */
