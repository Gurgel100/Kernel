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
	bool read, write, append, empty, create;
}vfs_mode_t;

typedef enum{
	VFS_INFO_FILESIZE, VFS_INFO_BLOCKSIZE, VFS_INFO_USEDBLOCKS, VFS_INFO_CREATETIME, VFS_INFO_ACCESSTIME, VFS_INFO_CHANGETIME
}vfs_fileinfo_t;

typedef uint64_t pid_t;
typedef uint64_t tid_t;

inline void *AllocPage(size_t Pages);
inline void FreePage(void *Address, size_t Pages);
inline void syscall_unusePage(void *Address, size_t Pages);

inline char syscall_getch();
inline void syscall_putch(unsigned char c);

inline pid_t syscall_createProcess(const char *path, const char *cmd, bool newConsole);
inline void syscall_exit(int status);
inline tid_t syscall_createThread(void *entry);
inline void syscall_exitThread(int status);

inline void *syscall_fopen(char *path, vfs_mode_t mode);
inline void syscall_fclose(void *stream);
inline size_t syscall_fread(void *stream, uint64_t start, size_t length, const void *buffer);
inline size_t syscall_fwrite(void *stream, uint64_t start, size_t length, const void *buffer);
inline uint64_t syscall_StreamInfo(void *stream, vfs_fileinfo_t info);

inline void syscall_sleep(uint64_t msec);

inline void syscall_getSysInfo(void *Struktur);

#endif /* SYSCALL_H_ */

#endif
