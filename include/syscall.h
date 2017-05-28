/*
 * syscall.h
 *
 *  Created on: 28.10.2014
 *      Author: pascal
 */

#ifndef BUILD_KERNEL

#ifndef SYSCALL_H_
#define SYSCALL_H_

#include "stddef.h"
#include "stdint.h"
#include "stdbool.h"

typedef struct{
	bool read, write, append, empty, create, directory;
}vfs_mode_t;

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

typedef uint64_t pid_t;
typedef uint64_t tid_t;

void *AllocPage(size_t Pages);
void FreePage(void *Address, size_t Pages);
void syscall_unusePage(void *Address, size_t Pages);

pid_t syscall_createProcess(const char *path, const char *cmd, const char **env, const char *stdin, const char *stdout, const char *stderr);
void syscall_exit(int status);
pid_t syscall_wait(pid_t pid, int *status);
tid_t syscall_createThread(void *entry, void *arg);
void syscall_exitThread(int status);

void *syscall_fopen(char *path, vfs_mode_t mode);
void syscall_fclose(void *stream);
size_t syscall_fread(void *stream, uint64_t start, size_t length, const void *buffer);
size_t syscall_fwrite(void *stream, uint64_t start, size_t length, const void *buffer);
uint64_t syscall_StreamInfo(void *stream, vfs_fileinfo_t info);
int syscall_mount(const char *mountpoint, const char *device);
int syscall_unmount(const char *mountpoint);

void syscall_sleep(uint64_t msec);

void syscall_getSysInfo(void *Struktur);

#endif /* SYSCALL_H_ */

#endif
