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

#ifdef BUILD_KERNEL

#ifndef VFS_H_
#define VFS_H_

#include "cdi/fs.h"
#include "cdi.h"
#include "stdbool.h"
#include "pm.h"
#include "refcount.h"

#define VFS_SEPARATOR	'/'
#define VFS_ROOT		"/"

#ifndef EOF
#define EOF -1
#endif

/**
 * @brief Type of device functions which can be called through vfs_device_t->function.
 */
typedef enum{
	/**
	 * Function which returns the type of the device. Must always be able to be called.
	 */
	VFS_DEV_FUNC_TYPE,

	/**
	 * Function to get the name of the device. Must always be able to be called.
	 */
	VFS_DEV_FUNC_NAME,

	/**
	 * Function to get the blocksize of a device. Only available if #VFS_DEV_CAP_BLOCKSIZE is set in the capabilities flags of the device.
	 */
	VFS_DEV_FUNC_BLOCKSIZE,

	/**
	 * Function to scan the device for partitions. Gets an open file id as first parameter. Only available if #VFS_DEV_CAP_PARTITIONS is set in the capabilities flags of the device.
	 */
	VFS_DEV_FUNC_SCAN_PARTITIONS,

	/**
	 * Function to mount a device. Returns a vfs_filesyste_t structure. Only available if #VFS_DEV_CAP_MOUNTABLE is set in the capabilities flags of the device.
	 */
	VFS_DEV_FUNC_MOUNT,

	/**
	 * Function to unmount a device. Only available if #VFS_DEV_CAP_MOUNTABLE is set in the capabilities flags of the device.
	 */
	VFS_DEV_FUNC_UMOUNT
}vfs_device_function_t;

/**
 * @brief Type of device
 */
typedef enum{
	/**
	 * Device is a storage device.
	 */
	VFS_DEVICE_STORAGE,

	/**
	 * Device is a partition.
	 */
	VFS_DEVICE_PARTITION,

	/**
	 * Device is a virtual device.
	 */
	VFS_DEVICE_VIRTUAL,
}vfs_device_type_t;

/**
 * @brief Capabilities of a device
 *
 * Capabilities are flags returned in a bitmask from the getCapabilities function in the vfs_device_t structure.
 */
typedef enum{
	/**
	 * Device supports the VFS_DEV_FUNC_BLOCKSIZE function.
	 */
	VFS_DEV_CAP_BLOCKSIZE	= 0x1,

	/**
	 * Device supports the VFS_DEV_FUNC_SCAN_PARTITIONS function.
	 */
	VFS_DEV_CAP_PARTITIONS	= 0x2,

	/**
	 * Device supports the VFS_DEV_FUNC_MOUNT and VFS_DEV_FUNC_UMOUNT functions.
	 */
	VFS_DEV_CAP_MOUNTABLE	= 0x4
}vfs_device_capabilities_t;

//Handler für Geräte
typedef size_t (*vfs_device_read_handler_t)(void *opaque, uint64_t start, size_t size, void *buffer);
typedef size_t (*vfs_device_write_handler_t)(void *opaque, uint64_t start, size_t size, const void *buffer);
typedef void *(*vfs_device_function_handler_t)(void *opaque, vfs_device_function_t function, ...);
typedef vfs_device_capabilities_t (*vfs_device_getCapabilities_handler_t)(void *opaque);

typedef struct{
	//Functionen zum Lesen und Schreiben
	vfs_device_read_handler_t read;
	vfs_device_write_handler_t write;

	//Hiermit können verschiedene Werte ausgelesen werden
	vfs_device_function_handler_t function;
	vfs_device_getCapabilities_handler_t getCapabilities;
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

typedef struct{
	struct cdi_fs_filesystem fs;
	REFCOUNT_FIELD;
	vfs_device_t *device;
	void *opaque;
}vfs_filesystem_t;

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
