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

inline void *AllocPage(size_t Pages);
inline void FreePage(void *Address, size_t Pages);

inline char syscall_getch();
inline void syscall_putch(unsigned char c);

inline void syscall_exit(int status);

inline vfs_stream_t *syscall_fopen(char *path, vfs_mode_t mode);
inline void syscall_fclose(vfs_stream_t *stream);
inline size_t syscall_fread(vfs_stream_t *stream, uint64_t start, size_t length, const void *buffer);
inline size_t syscall_fwrite(vfs_stream_t *stream, uint64_t start, size_t length, const void *buffer);
inline uint64_t syscall_StreamInfo(vfs_stream_t *stream, vfs_fileinfo_t info);

#endif /* SYSCALL_H_ */

#endif
