/*
 * syscall_types.h
 *
 *  Created on: 17.06.2017
 *      Author: pascal
 */

#ifndef BITS_SYS_TYPES_H_
#define BITS_SYS_TYPES_H_

#include <stdbool.h>
#include <stdint.h>

typedef _size_t size_t;

typedef _pid_t	pid_t;
typedef _tid_t	tid_t;

typedef _time_t	time_t;

typedef struct{
	bool read, write, append, empty, create, directory;
}vfs_mode_t;

typedef enum{
	VFS_INFO_FILESIZE, VFS_INFO_BLOCKSIZE, VFS_INFO_USEDBLOCKS, VFS_INFO_CREATETIME, VFS_INFO_ACCESSTIME, VFS_INFO_CHANGETIME, VFS_INFO_ATTRIBUTES
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
	uint64_t	physSpeicher;
	uint64_t	physFree;
	uint64_t	Uptime;
}SIS;

#endif /* BITS_SYS_TYPES_H_ */
