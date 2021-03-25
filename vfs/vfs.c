/*
 * vfs.c
 *
 *  Created on: 13.08.2013
 *      Author: pascal
 */

#include "vfs.h"
#include "stdint.h"
#include "stddef.h"
#include "string.h"
#include "stdlib.h"
#include "list.h"
#include "display.h"
#include "stdio.h"
#include "lock.h"
#include "assert.h"
#include "pm.h"
#include "hashmap.h"
#include "ctype.h"
#include "path.h"
#include <hash_helpers.h>
#include <refcount.h>

#define MIN(a, b)	((a < b) ? a : b)

struct vfs_stream{
	vfs_node_t *node;
	REFCOUNT_FIELD;
	vfs_mode_t mode;
};

//Ein Stream vom Userspace hat eine ID, der auf einen Stream des Kernels gemappt ist
typedef struct{
	vfs_file_t id;
	vfs_stream_t *stream;
	vfs_mode_t mode;
}vfs_userspace_stream_t;

typedef struct
{
	void *buffer;
	size_t size;
	size_t *bytesWritten;
}vfs_visit_childs_context_t;

static vfs_node_dir_t root;
static hashmap_t *filesystem_drivers;

static size_t getDirs(char ***Dirs, const char *Path)
{
	size_t i;
	char *tmp;
	//Erst Pfad sichern
	char *path_copy = strdup(Path);
	char *tmpPath = path_copy;

	if(!*Dirs)
		*Dirs = malloc(sizeof(char*));

	char *first_token = strtok_s(&tmpPath, VFS_ROOT);
	if(first_token == NULL) return 0;

	(*Dirs)[0] = strdup(first_token);
	for(i = 1; ; i++)
	{
		if((tmp = strtok_s(&tmpPath, VFS_ROOT)) == NULL)
			break;
		*Dirs = realloc(*Dirs, sizeof(char*) * (i + 1));
		(*Dirs)[i] = strdup(tmp);
	}
	free(path_copy);
	return i;
}

static void freeDirs(char ***Dirs, size_t size)
{
	size_t i;
	for(i = 0; i < size; i++)
		free((*Dirs)[i]);
	free(*Dirs);
}

static uint64_t streamid_hash(const void *key, __attribute__((unused)) void *context)
{
	return (uint64_t)key;
}

static bool streamid_equal(const void *a, const void *b, __attribute__((unused)) void *context)
{
	return a == b;
}

/*
 * Wird aufgerufen, wenn ein Stream keine Verweise mehr hat.
 */
static void vfs_stream_closed(const void *s)
{
	vfs_stream_t *stream = (vfs_stream_t*)s;

	//Remove stream from opened stream hashmap of node
	LOCKED_TASK(stream->node->lock, hashmap_delete(stream->node->streams, stream));

	free(stream);
}

/*
 * Wird für die Userspace Hashtable verwendet. Darf nicht auf current_thread->lock locken.
 */
static void vfs_userspace_stream_free(const void *s)
{
	vfs_userspace_stream_t *stream = (vfs_userspace_stream_t*)s;
	vfs_Close(stream->stream);
	free(stream);
}

static vfs_file_t getNextUserspaceStreamID(process_t *p)
{
	vfs_file_t id = 0;
	while(id == -1ul || LOCKED_RESULT(p->lock, hashmap_search(p->streams, (void*)id, NULL)))
	{
		id++;
	}
	return id;
}

static vfs_node_t *resolveLink(vfs_node_t *node)
{
	while(node->type == VFS_NODE_LINK)
	{
		node = ((vfs_node_link_t*)node)->getLink((vfs_node_link_t*)node);
	}
	return node;
}

/*
 * Überprüft den Pfad auf ungültige Zeichen
 * Parameter:	path = Pfad, der überprüft werden soll
 * Rückgabe:	Ob der Pfad gültig ist
 */
static bool check_path(const char *path)
{
	char c;
	while((c = *path++) != '\0')
	{
		if(iscntrl(c) || c == '|' || c == '\\' || c == '*' || c == '\"' || c == '?')
			return false;
	}
	return true;
}


/*
 * Finde die Node auf die der Pfad zeigt. Der Pfad muss absolut abgeben werden.
 * Parameter:	Path = Absoluter Pfad
 */
static vfs_node_t *getNode(const char *Path, bool resolveLastLink)
{
	char **Dirs = NULL;
	bool isModule = *Path == ':';
	size_t NumDirs;
	vfs_node_t *node;

	//Welche Ordner?
	NumDirs = getDirs(&Dirs, Path + isModule);
	if(isModule)
	{
		vfs_filesystem_driver_t *driver;
		if(NumDirs < 1 || !hashmap_search(filesystem_drivers, Dirs[0], (void**)&driver) || !(driver->info & VFS_FILESYSTEM_INFO_MODULE))
		{
			freeDirs(&Dirs, NumDirs);
			return NULL;
		}
		node = &driver->get_root()->base;
	}
	else
	{
		vfs_filesystem_t *fs = list_get(root.mounts, 0);
		if(fs != NULL)
			node = &fs->root->base;
		else
			node = &root.base;
	}

	for(size_t i = isModule; i < NumDirs; i++)
	{
		node = resolveLink(node);
		assert(node->type != VFS_NODE_LINK);
		if(node->type != VFS_NODE_DIR)
		{
			freeDirs(&Dirs, NumDirs);
			return NULL;
		}

		vfs_node_dir_t *dnode = (vfs_node_dir_t*)node;
		if((node = dnode->findChild(dnode, Dirs[i])) == NULL)
		{
			freeDirs(&Dirs, NumDirs);
			return NULL;
		}

		if(node->type == VFS_NODE_DIR)
		{
			vfs_filesystem_t *fs = list_get(((vfs_node_dir_t*)node)->mounts, 0);
			if(fs != NULL)
				node = &fs->root->base;
		}
	}

	freeDirs(&Dirs, NumDirs);
	return resolveLastLink ? resolveLink(node) : node;
}

static int createDirEntry(const char *path, vfs_node_type_t type)
{
	if(path == NULL || strlen(path) == 0 || !check_path(path))
		return -1;

	char *name;

	path = path_removeLast(path, &name);

	vfs_node_t *node = getNode(path, true);

	free((char*)path);

	if(node == NULL || node->type != VFS_NODE_DIR)
	{
		free(name);
		return -1;
	}

	int res = ((vfs_node_dir_t*)node)->createChild((vfs_node_dir_t*)node, type, name);

	free(name);

	return res;
}

void vfs_Init(void)
{
	filesystem_drivers = hashmap_create(string_hash, string_hash, string_equal, NULL, NULL, NULL, NULL, 1);
	assert(filesystem_drivers != NULL);

	vfs_node_dir_init(&root, VFS_ROOT);

	devfs_init();
}

static ERROR_TYPE_POINTER(vfs_stream_t) getOrCreateStream(vfs_node_t *node, vfs_mode_t mode)
{
	return LOCKED_RESULT(node->lock, {
		vfs_stream_t *stream;
		if(!hashmap_search(node->streams, &mode, (void**)&stream) || REFCOUNT_RETAIN(stream) == NULL)
		{
			stream = calloc(1, sizeof(*stream));
			if(stream == NULL)
				return ERROR_RETURN_POINTER_ERROR(vfs_stream_t, E_NO_MEMORY);

			stream->mode = mode;
			stream->node = node;
			REFCOUNT_INIT(stream, vfs_stream_closed);

			hashmap_set(stream->node->streams, &stream->mode, stream);
		}
		ERROR_RETURN_POINTER_VALUE(vfs_stream_t, stream);
	});
}

static bool checkMode(vfs_node_t *node, vfs_mode_t mode)
{
	return !!(mode & VFS_MODE_DIR) == (node->type == VFS_NODE_DIR);
}

/*
 * Eine Datei öffnen
 * Parameter:	path = Pfad zur Datei
 * 				mode = Modus, in der die Datei geöffnet werden soll
 */
ERROR_TYPE_POINTER(vfs_stream_t) vfs_Open(const char *path, vfs_mode_t mode)
{
	if(path == NULL || strlen(path) == 0 || !check_path(path) || !(mode & (VFS_MODE_READ | VFS_MODE_WRITE))
		|| (mode & (VFS_MODE_WRITE | VFS_MODE_DIR)) == (VFS_MODE_WRITE | VFS_MODE_DIR)
		|| (mode & (VFS_MODE_DIR | VFS_MODE_EXEC)) == (VFS_MODE_DIR | VFS_MODE_EXEC)
		|| (mode & (VFS_MODE_DIR | VFS_MODE_TRUNCATE)) == (VFS_MODE_DIR | VFS_MODE_TRUNCATE))
		return ERROR_RETURN_POINTER_ERROR(vfs_stream_t, E_INVALID_ARGUMENT);

	if(mode & VFS_MODE_CREATE)
		createDirEntry(path, VFS_NODE_FILE);

	vfs_node_t *node = getNode(path, true);
	if(node == NULL) return ERROR_RETURN_POINTER_ERROR(vfs_stream_t, E_INVALID_ARGUMENT);
	if (!checkMode(node, mode)) return ERROR_RETURN_POINTER_ERROR(vfs_stream_t, E_NOT_DIR);

	return getOrCreateStream(node, mode);
}

ERROR_TYPE_POINTER(vfs_stream_t) vfs_Reopen(vfs_stream_t *stream, vfs_mode_t mode)
{
	if(!(mode & (VFS_MODE_READ | VFS_MODE_WRITE)) || (mode & (VFS_MODE_WRITE | VFS_MODE_DIR)) == (VFS_MODE_WRITE | VFS_MODE_DIR))
		return ERROR_RETURN_POINTER_ERROR(vfs_stream_t, E_INVALID_ARGUMENT);

	vfs_node_t *node = stream->node;
	if(stream->mode == mode) {
		if (REFCOUNT_RETAIN(stream) != NULL) return ERROR_RETURN_POINTER_VALUE(vfs_stream_t, stream);
	} else {
		vfs_Close(stream);
	}

	return getOrCreateStream(node, mode);
}

/*
 * Eine Datei schliessen
 * Parameter:	stream = Stream der geschlossen werden soll
 */
void vfs_Close(vfs_stream_t *stream)
{
	//Reservierten Stream freigeben
	REFCOUNT_RELEASE(stream);
}

static bool visitChildsCallback(vfs_node_t *node, void *context)
{
	vfs_visit_childs_context_t *info = context;
	size_t name_len = strlen(node->name);
	size_t entry_size = sizeof(vfs_userspace_direntry_t) + name_len + 1;
	if(*info->bytesWritten + entry_size > info->size)
		return false;

	vfs_userspace_direntry_t *entry = info->buffer + *info->bytesWritten;
	switch(node->type)
	{
		case VFS_NODE_DIR:
			entry->type = UDT_DIR;
		break;
		case VFS_NODE_FILE:
			entry->type = UDT_FILE;
		break;
		case VFS_NODE_LINK:
			entry->type = UDT_LINK;
		break;
		case VFS_NODE_DEV:
			entry->type = UDT_DEV;
		break;
		default:
			entry->type = UDT_UNKNOWN;
	}
	entry->size = entry_size;
	strcpy(entry->name, node->name);

	*info->bytesWritten += entry_size;

	return true;
}

/*
 * Eine Datei lesen
 * Parameter:	Path = Pfad zur Datei als String
 * 				start = Anfangsbyte, an dem angefangen werden soll zu lesen
 * 				length = Anzahl der Bytes, die gelesen werden sollen
 * 				Buffer = Buffer in den die Bytes geschrieben werden
 */
size_t vfs_Read(vfs_stream_t *stream, uint64_t start, size_t length, void *buffer)
{
	if(buffer == NULL)
		return 0;

	size_t sizeRead = 0;

	//Erst überprüfen ob der Stream zum lesen geöffnet wurde
	if(!(stream->mode & VFS_MODE_READ))
		return 0;

	vfs_node_t *node = stream->node;
	switch(node->type)
	{
		case VFS_NODE_DIR:
		{
			vfs_visit_childs_context_t info = {
				.buffer = buffer,
				.size = length,
				.bytesWritten = &sizeRead
			};
			((vfs_node_dir_t*)node)->visitChilds(((vfs_node_dir_t*)node), start, visitChildsCallback, &info);
		}
		break;
		case VFS_NODE_FILE:
			sizeRead = ((vfs_node_file_t*)node)->read((vfs_node_file_t*)node, start, length, buffer);
		break;
		case VFS_NODE_DEV:
			sizeRead = ((vfs_node_dev_t*)node)->read((vfs_node_dev_t*)node, start, length, buffer);
		break;
	}

	assert(sizeRead <= length);
	return sizeRead;
}

size_t vfs_Write(vfs_stream_t *stream, uint64_t start, size_t length, const void *buffer)
{
	if(buffer == NULL)
		return 0;

	size_t sizeWritten = 0;

	//Erst überprüfen ob der Stream zum schreiben geöffnet wurde
	if(!(stream->mode & VFS_MODE_WRITE))
		return 0;

	vfs_node_t *node = stream->node;
	switch(node->type)
	{
		case VFS_NODE_FILE:
			sizeWritten = ((vfs_node_file_t*)node)->write(((vfs_node_file_t*)node), start, length, buffer);
		break;
		case VFS_NODE_DEV:
			sizeWritten = ((vfs_node_dev_t*)node)->write(((vfs_node_dev_t*)node), start, length, buffer);
		break;
	}

	assert(sizeWritten <= length);
	return sizeWritten;
}

//TODO: Erbe alle geöffneten Stream vom Vaterprozess
int vfs_initUserspace(process_t *parent, process_t *p, const char *stdin, const char *stdout, const char *stderr)
{
	assert(p != NULL && ((parent == NULL && stdin != NULL && stdout != NULL && stderr != NULL) || parent != NULL));
	if((p->streams = hashmap_create(streamid_hash, streamid_hash, streamid_equal, NULL, vfs_userspace_stream_free, NULL, NULL, 3)) == NULL)
		return 1;

	ERROR_TYPE_POINTER(vfs_stream_t) kstream;
	vfs_userspace_stream_t *stream;
	if(parent != NULL && stdin == NULL)
	{
		LOCKED_TASK(parent->lock, hashmap_search(parent->streams, (void*)0, (void**)&stream));
		assert(stream != NULL);
		kstream = vfs_Reopen(stream->stream, VFS_MODE_READ);
	}
	else
	{
		kstream = vfs_Open(stdin, VFS_MODE_READ);
	}
	if(ERROR_DETECT(kstream))
		return 0;
	stream = malloc(sizeof(vfs_userspace_stream_t));
	if(stream == NULL)
		return 0;
	stream->id = 0;
	stream->mode = VFS_MODE_READ;
	stream->stream = ERROR_GET_VALUE(kstream);
	hashmap_set(p->streams, (void*)0, stream);

	if(parent != NULL && stdout == NULL)
	{
		LOCKED_TASK(parent->lock, hashmap_search(parent->streams, (void*)1, (void**)&stream));
		assert(stream != NULL);
		kstream = vfs_Reopen(stream->stream, VFS_MODE_WRITE);
	}
	else
	{
		kstream = vfs_Open(stdout, VFS_MODE_WRITE);
	}
	if(ERROR_DETECT(kstream))
		return 0;
	stream = malloc(sizeof(vfs_userspace_stream_t));
	if(stream == NULL)
		return 0;
	stream->id = 1;
	stream->mode = VFS_MODE_WRITE;
	stream->stream = ERROR_GET_VALUE(kstream);
	hashmap_set(p->streams, (void*)1, stream);

	if(parent != NULL && stderr == NULL)
	{
		LOCKED_TASK(parent->lock, hashmap_search(parent->streams, (void*)2, (void**)&stream));
		assert(stream != NULL);
		kstream = vfs_Reopen(stream->stream, VFS_MODE_WRITE);
	}
	else
	{
		kstream = vfs_Open(stderr, VFS_MODE_WRITE);
	}
	if(ERROR_DETECT(kstream))
		return 0;
	stream = malloc(sizeof(vfs_userspace_stream_t));
	if(stream == NULL)
		return 0;
	stream->id = 2;
	stream->mode = VFS_MODE_WRITE;
	stream->stream = ERROR_GET_VALUE(kstream);
	hashmap_set(p->streams, (void*)2, stream);
	return 1;
}

void vfs_deinitUserspace(process_t *p)
{
	assert(p != NULL);
	hashmap_destroy(p->streams);
}

/*
 * Gibt die Metainformationen einer Datei zurück
 * Parameter:	stream = stream dessen Grösse abgefragt wird (muss eine Datei sein)
 * Rückgabe:	Grösse des Streams oder 0 bei Fehler
 */
uint64_t vfs_getFileinfo(vfs_stream_t *stream, vfs_fileinfo_t info)
{
	uint64_t value;
	if(stream->node->getAttribute(stream->node, info, &value))
		return 0;
	else
		return value;
}

/**
 * Sets metainformations of a file
 * \param stream Stream of which the metainformation should be set
 * \param info Information which should be set
 * \param value Value to be set
 */
void vfs_setFileinfo(vfs_stream_t *stream, vfs_fileinfo_t info, uint64_t value)
{
	stream->node->setAttribute(stream->node, info, value);
}

int vfs_truncate(const char *path, size_t size)
{
	if(path == NULL || strlen(path) == 0 || !check_path(path))
		return -1;

	vfs_node_t *node = getNode(path, true);

//	switch(node->type)
//	{
//		case TYPE_MOUNT:
//		{
//			struct cdi_fs_stream stream;
//			stream.fs = &node->fs->fs;
//			stream.res = getRes(&stream, remPath);
//			free(remPath);
//			//Löse symlinks auf
//			while(stream.res != NULL && stream.res->link != NULL)
//			{
//				const char *link_path = stream.res->link->read_link(&stream);
//				stream.res = getRes(&stream, link_path);
//			}
//
//			if(stream.res == NULL || !stream.res->flags.write || stream.res->dir == NULL)
//				return -1;
//
//			if(!stream.res->file->truncate(&stream, size))
//				return -1;
//		}
//		break;
//		case TYPE_DIR:
//			free(remPath);
//			return -1;
//		break;
//		case TYPE_DEV:
//			free(remPath);
//			return -1;
//		break;
//		case TYPE_LINK:
//			assert(false);
//		break;
//		case TYPE_FILE:
//			//TODO
//			//Not supported
//			free(remPath);
//			return -1;
//		break;
//	}

	if(node->type != VFS_NODE_FILE)
		return -1;

	vfs_node_file_t *fnode = (vfs_node_file_t*)node;

	return fnode->truncate(fnode, size);
}

int vfs_createDir(const char *path)
{
	return createDirEntry(path, VFS_NODE_DIR);
}

static void findFilesystemDriver(const void *key, const void *obj, void *context)
{
	vfs_filesystem_driver_t *driver = (vfs_filesystem_driver_t*)obj;
	void **tmp = context;
	vfs_filesystem_driver_t **outdriver = tmp[0];
	vfs_stream_t *dev = tmp[1];

	vfs_filesystem_t fs = {
		.driver = driver
	};

	if(*outdriver == NULL && !driver->probe(&fs, dev))
	{
		*outdriver = driver;
	}
}

static void freeFilesystem(const void *fs_p)
{
	vfs_filesystem_t *fs = (vfs_filesystem_t*)fs_p;
	fs->driver->umount(fs);
	free(fs);
}

/*
 * Mountet ein Dateisystem (fs) an den entsprechenden Mountpoint (Mount)
 * Parameter:	Mount = Mountpoint (Pfad)
 * 				Dev = Pfad zum Gerät
 * Rückgabe:	!0 bei Fehler
 */
int vfs_Mount(const char *mountpath, const char *devpath, const char *filesystem)
{
	vfs_node_dir_t *mount;
	vfs_node_t *devNode;
	vfs_filesystem_driver_t *driver = NULL;
	vfs_filesystem_t *fs = NULL;
	vfs_stream_t *dev_stream = NULL;
	bool ignoreDev = false;

	if((mount = (vfs_node_dir_t*)getNode(mountpath, true)) == NULL)
		return 2;

	if(mount->base.type != VFS_NODE_DIR)
		return 6;

	if(filesystem != NULL)
	{
		if(!hashmap_search(filesystem_drivers, filesystem, (void**)&driver))
			return 1;
		ignoreDev = driver->info & VFS_FILESYSTEM_INFO_VIRTUAL;
	}

	if(!ignoreDev)
	{
		if((devNode = getNode(devpath, true)) == NULL)
		{
			return 3;
		}
		else
		{
			if(devNode->type != VFS_NODE_DEV)
				return 3;

			vfs_node_dev_t *dnode = (vfs_node_dev_t*)devNode;
			if((dnode->type & VFS_DEVICE_CHARACTER))
				return 3;

			if(dnode->fs != NULL)
			{
				if(REFCOUNT_RETAIN(dnode->fs) != NULL)
				{
					fs = dnode->fs;
					goto mount_end;
				}
			}

			ERROR_TYPE_POINTER(vfs_stream_t) dev_stream_ret = vfs_Open(devpath, VFS_MODE_READ | VFS_MODE_WRITE);
			if (ERROR_DETECT(dev_stream_ret))
				return 3;
			dev_stream = ERROR_GET_VALUE(dev_stream_ret);
		}

		void *tmp[2] = {&driver, dev_stream};
		hashmap_visit(filesystem_drivers, findFilesystemDriver, tmp);
		if(driver == NULL)
			return 5;
	}

	fs = calloc(1, sizeof(vfs_filesystem_t));
	if(fs == NULL)
		return 1;
	fs->driver = driver;

	int res = driver->mount(fs, dev_stream);
	if(res)
	{
		free(fs);
		return res;
	}

	REFCOUNT_INIT(fs, freeFilesystem);

mount_end:

	assert(fs != NULL);

	LOCKED_TASK(mount->base.lock, list_push(mount->mounts, fs));

	printf("mounted device %s on %s (%x)\n", devpath ? : "(ignored)", mount->base.name, mount);

	return 0;
}

/*
 * Unmountet ein Dateisystem am entsprechenden Mountpoint (Mount)
 * Parameter:	Mount = Mountpoint (Pfad)
 * Rückgabe:	!0 bei Fehler
 */
int vfs_Unmount(const char *mountpath)
{
	vfs_node_dir_t *mount = (vfs_node_dir_t*)getNode(mountpath, true);
	if(!mount || mount->base.type != VFS_NODE_DIR)
		return 1;

	vfs_filesystem_t *fs = LOCKED_RESULT(mount->base.lock, list_pop(mount->mounts));
	if(fs == NULL)
		return 2;

	REFCOUNT_RELEASE(fs);

	return 0;
}

static bool findRootDev(vfs_node_t *node, void* context)
{
	if(node->type == VFS_NODE_DEV)
	{
		const char path_format[] = ":devfs/%s";
		size_t pathSize = snprintf(NULL, 0, path_format, node->name);
		char path[pathSize + 1];
		snprintf(path, pathSize + 1, path_format, node->name);
		if(vfs_Mount(VFS_ROOT, path, NULL) == 0)
		{
			printf("Mounted device: %s\n", node->name);
			ERROR_TYPE_POINTER(vfs_stream_t) file = vfs_Open("/kernel", VFS_MODE_READ);
			if(ERROR_DETECT(file))
			{
				vfs_Close(ERROR_GET_VALUE(file));
				printf("Found root device: %s\n", node->name);
				return false;
			}
			else
			{
				vfs_Unmount(VFS_ROOT);
			}
		}
	}
	return true;
}

/*
 * Mountet das erste Gerät
 * Rückgabe:	!0 bei Fehler
 */
int vfs_MountRoot(void)
{
	//Use a temporary mount point to search for device
	vfs_filesystem_driver_t *devfs_driver;
	if(!hashmap_search(filesystem_drivers, "devfs", (void**)&devfs_driver))
		return 1;
	vfs_node_dir_t *devfs_root = devfs_driver->get_root();
	return devfs_root->visitChilds(devfs_root, 0, findRootDev, NULL);
}

int vfs_registerFilesystemDriver(vfs_filesystem_driver_t *driver)
{
	hashmap_set(filesystem_drivers, driver->name, driver);

	return 0;
}

//Syscalls
//TODO: Define errors correctly via macros
vfs_file_t vfs_syscall_open(const char *path, vfs_mode_t mode)
{
	assert(currentProcess != NULL);
	vfs_userspace_stream_t *stream = malloc(sizeof(vfs_userspace_stream_t));
	if(stream == NULL)
		return -1;
	ERROR_TYPE_POINTER(vfs_stream_t) kstream = vfs_Open(path, mode);
	if(ERROR_DETECT(kstream))
	{
		free(stream);
		return -1;
	}
	stream->id = getNextUserspaceStreamID(currentProcess);
	stream->mode = mode;
	stream->stream = ERROR_GET_VALUE(kstream);
	LOCKED_TASK(currentProcess->lock, hashmap_set(currentProcess->streams, (void*)stream->id, stream));
	assert(LOCKED_RESULT(currentProcess->lock, hashmap_search(currentProcess->streams, (void*)stream->id, NULL)));
	return stream->id;
}

void vfs_syscall_close(vfs_file_t streamid)
{
	assert(currentProcess != NULL);
	LOCKED_TASK(currentProcess->lock, hashmap_delete(currentProcess->streams, (void*)streamid));
}

size_t vfs_syscall_read(vfs_file_t streamid, uint64_t start, size_t length, void *buffer)
{
	vfs_userspace_stream_t *stream;
	assert(currentProcess != NULL);
	if(!LOCKED_RESULT(currentProcess->lock, hashmap_search(currentProcess->streams, (void*)streamid, (void**)&stream)))
		return 0;
	if(!vmm_userspacePointerValid(buffer, length))
		return 0;
	if(!(stream->mode & VFS_MODE_READ))
		return 0;
	return vfs_Read(stream->stream, start, length, buffer);
}

size_t vfs_syscall_write(vfs_file_t streamid, uint64_t start, size_t length, const void *buffer)
{
	vfs_userspace_stream_t *stream;
	assert(currentProcess != NULL);
	if(!LOCKED_RESULT(currentProcess->lock, hashmap_search(currentProcess->streams, (void*)streamid, (void**)&stream)))
		return 0;
	if(!vmm_userspacePointerValid(buffer, length))
		return 0;
	if(!(stream->mode & VFS_MODE_WRITE))
		return 0;
	return vfs_Write(stream->stream, start, length, buffer);
}

uint64_t vfs_syscall_getFileinfo(vfs_file_t streamid, vfs_fileinfo_t info)
{
	vfs_userspace_stream_t *stream;
	assert(currentProcess != NULL);
	if(!LOCKED_RESULT(currentProcess->lock, hashmap_search(currentProcess->streams, (void*)streamid, (void**)&stream)))
		return 0;
	return vfs_getFileinfo(stream->stream, info);
}

void vfs_syscall_setFileinfo(vfs_file_t streamid, vfs_fileinfo_t info, uint64_t value)
{
	vfs_userspace_stream_t *stream;
	assert(currentProcess != NULL);
	if(!LOCKED_RESULT(currentProcess->lock, hashmap_search(currentProcess->streams, (void*)streamid, (void**)&stream)))
		return;
	vfs_setFileinfo(stream->stream, info, value);
}

int vfs_syscall_truncate(const char *path, size_t size)
{
	if(path == NULL || !vmm_userspacePointerValid(path, strlen(path)))
		return -1;
	return vfs_truncate(path, size);
}

int vfs_syscall_mkdir(const char *path)
{
	if(path == NULL || !vmm_userspacePointerValid(path, strlen(path)))
		return -1;
	return vfs_createDir(path);
}

int vfs_syscall_mount(const char *mountpoint, const char *device, const char *filesystem)
{
	if(!vmm_userspacePointerValid(mountpoint, strlen(mountpoint)))
		return -1;
	if(!vmm_userspacePointerValid(device, strlen(device)))
		return -1;
	if(filesystem != NULL && !vmm_userspacePointerValid(filesystem, strlen(filesystem)))
		return -1;
	return vfs_Mount(mountpoint, device, filesystem);
}

int vfs_syscall_unmount(const char *mountpoint)
{
	if(!vmm_userspacePointerValid(mountpoint, strlen(mountpoint)))
		return -1;
	return vfs_Unmount(mountpoint);
}
