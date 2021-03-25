/*
 * dirent.c
 *
 *  Created on: 21.03.2017
 *      Author: pascal
 */

#include "dirent.h"
#include "errno.h"
#include "stdlib.h"
#include "assert.h"
#include "stdbool.h"
#include "limits.h"
#include "string.h"
#include "bits/sys_types.h"
#ifdef BUILD_KERNEL
#include "vfs.h"
#else
#include "syscall.h"
#endif

#define MIN(a, b)	((a < b) ? a : b)
#define DIR_BUFSIZE	1024

struct dirstream{
	id_t stream_id;
	uint64_t pos, buffer_pos;
	void *buffer;
	size_t buffer_size, buffer_used;
	struct dirent *dirent_buffer;
};

int alphasort(const struct dirent **d1, const struct dirent **d2)
{
	//NOTE: verwende strcoll anstatt strcmp sobald implementiert
	return strcmp((*d1)->d_name, (*d2)->d_name);
}

int closedir(DIR* dirp)
{
	if(dirp == NULL)
		return -1;

#ifdef BUILD_KERNEL
	vfs_Close(dirp->stream_id);
#else
	syscall_fclose(dirp->stream_id);
#endif
	free(dirp->dirent_buffer);
	free(dirp->buffer);
	free(dirp);

	return 0;
}

DIR* opendir(const char* dirname)
{
	if(strlen(dirname) == 0)
	{
		errno = ENOENT;
		return NULL;
	}

	DIR *dir = calloc(1, sizeof(DIR));
	if(dir == NULL)
		goto fail1;

	//Versuche die Buffer anzulegen
	dir->buffer = malloc(DIR_BUFSIZE);
	if(dir->buffer != NULL)
		dir->buffer_size = DIR_BUFSIZE;

	dir->dirent_buffer = malloc(sizeof(struct dirent) + NAME_MAX + 1);
	if(dir->dirent_buffer == NULL)
		goto fail2;

	vfs_mode_t m = VFS_MODE_READ | VFS_MODE_DIR;
#ifdef BUILD_KERNEL
	ERROR_TYPE_POINTER(vfs_stream_t) stream_ret = vfs_Open(dirname, m);
	dir->stream_id = ERROR_GET_VALUE(stream_ret);
	if(!ERROR_DETECT(stream_ret)) return dir;
#else
	dir->stream_id = syscall_fopen(dirname, m);
	if(dir->stream_id != -1ul) return dir;
#endif

fail3:
	free(dir->dirent_buffer);
fail2:
	free(dir->buffer);
	free(dir);
fail1:
	return NULL;
}

struct dirent* readdir(DIR* dirp)
{
	struct dirent *dirent;
	if(readdir_r(dirp, dirp->dirent_buffer, &dirent) == 0)
		return dirent;
	else
		return NULL;
}

int readdir_r(DIR* dirp, struct dirent* entry, struct dirent** result)
{
	*result = NULL;
	if(dirp == NULL || entry == NULL)
		return -1;

	void *buffer = dirp->buffer;
	size_t buffer_size = dirp->buffer_size;
	bool internal_buffer = dirp->buffer == NULL || dirp->buffer_size == 0;

	if(internal_buffer)
	{
		buffer_size = 1024;
		buffer = __builtin_alloca(buffer_size);
	}
	else if(dirp->buffer_used > 0 && dirp->buffer_pos <= dirp->pos)
	{
		vfs_userspace_direntry_t *entries = dirp->buffer;
		//Überprüfe, ob pos noch im Buffer ist
		size_t i = dirp->buffer_pos;
		while(entries < (vfs_userspace_direntry_t*)((uintptr_t)dirp->buffer + dirp->buffer_used) && i < dirp->pos)
		{
			entries = (vfs_userspace_direntry_t*)((uintptr_t)entries + entries->size);
			i++;
		}

		if(entries < (vfs_userspace_direntry_t*)((uintptr_t)dirp->buffer + dirp->buffer_used))
		{
			entry->d_off = dirp->pos;
			entry->d_reclen = sizeof(struct dirent) + MIN(entries->size - sizeof(vfs_userspace_direntry_t), NAME_MAX + 1);
			switch(entries->type)
			{
				case UDT_DIR:
					entry->d_type = DT_DIR;
				break;
				case UDT_FILE:
					entry->d_type = DT_FILE;
				break;
				case UDT_LINK:
					entry->d_type = DT_LINK;
				break;
				case UDT_DEV:
					entry->d_type = DT_DEV;
				break;
				case UDT_UNKNOWN:
				default:
					entry->d_type = DT_UNKNOWN;
				break;
			}
			strncpy(entry->d_name, entries->name, NAME_MAX);
			entry->d_name[NAME_MAX] = '\0';

			assert(strlen(entry->d_name) == entry->d_reclen - sizeof(struct dirent) - 1);

			dirp->pos++;

			*result = entry;

			return 0;
		}
	}

	assert(buffer != NULL);

	size_t read_bytes;
#ifdef BUILD_KERNEL
	read_bytes = vfs_Read(dirp->stream_id, dirp->pos, buffer_size, buffer);
#else
	read_bytes = syscall_fread(dirp->stream_id, dirp->pos, buffer_size, buffer);
#endif

	if(read_bytes == 0)
		return 0;	//EOF

	vfs_userspace_direntry_t *entries = buffer;
	entry->d_off = dirp->pos;
	entry->d_reclen = sizeof(struct dirent) + MIN(entries->size - sizeof(vfs_userspace_direntry_t), NAME_MAX + 1);
	switch(entries->type)
	{
		case UDT_DIR:
			entry->d_type = DT_DIR;
		break;
		case UDT_FILE:
			entry->d_type = DT_FILE;
		break;
		case UDT_LINK:
			entry->d_type = DT_LINK;
		break;
		case UDT_DEV:
			entry->d_type = DT_DEV;
		break;
		case UDT_UNKNOWN:
		default:
			entry->d_type = DT_UNKNOWN;
		break;
	}
	strncpy(entry->d_name, entries->name, NAME_MAX);
	entry->d_name[NAME_MAX] = '\0';

	assert(strlen(entry->d_name) == entry->d_reclen - sizeof(struct dirent) - 1);

	if(!internal_buffer)
	{
		dirp->buffer_used = read_bytes;
		dirp->buffer_pos = dirp->pos;
		assert(dirp->buffer_used <= dirp->buffer_size);
	}

	dirp->pos++;

	*result = entry;

	return 0;
}

void rewinddir(DIR* dirp)
{
	if(dirp != NULL)
		dirp->pos = 0;
}

int scandir(const char *dir, struct dirent ***namelist,
		int (*sel)(const struct dirent*),
		int (*compar)(const struct dirent**, const struct dirent**))
{
	DIR *d = opendir(dir);
	if(d == NULL)
		return -1;

	size_t count = 2;
	size_t position = 0;
	struct dirent *dirent;

	*namelist = malloc(count * sizeof(struct dirent*));
	if(*namelist == NULL)
	{
		closedir(d);
		return -1;
	}

	while((dirent = readdir(d)) != NULL)
	{
		if(sel != NULL && !sel(dirent)) continue;
		if(position >= count)
		{
			void *newpointer = realloc(*namelist, 2 * count * sizeof(struct dirent*));
			if(newpointer != NULL)
				count *= 2;
			else
			{
				//Versuche nochmal
				newpointer = realloc(*namelist, (count + 1) * sizeof(struct dirent*));
				if(newpointer != NULL) count++;
				else goto fail;
			}
			*namelist = newpointer;
		}

		(*namelist)[position] = malloc(dirent->d_reclen);
		if((*namelist)[position] == NULL) goto fail;
		memcpy((*namelist)[position], dirent, dirent->d_reclen);
		position++;
	}

	if(compar != NULL)
		qsort(*namelist, position, sizeof(**namelist), (int(*)(const void*, const void*))compar);

	closedir(d);
	return position;

fail:
	{
		size_t i;
		for(i = 0; i < position; i++)
			free((*namelist)[i]);
		free(*namelist);
		closedir(d);
		return -1;
	}
}

void seekdir(DIR* dirp, long loc)
{
	if(dirp != NULL)
		dirp->pos = loc;
}

long telldir(DIR* dirp)
{
	if(dirp == NULL)
		return 0;
	return dirp->pos;
}
