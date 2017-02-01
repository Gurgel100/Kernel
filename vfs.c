/*
 * vfs.c
 *
 *  Created on: 13.08.2013
 *      Author: pascal
 */

#ifdef BUILD_KERNEL

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

#define MAX_RES_BUFFER	100		//Anzahl an Ressourcen, die maximal geladen werden. Wenn der Buffer voll ist werden nicht benötigte Ressourcen überschrieben

#define VFS_MODE_READ	0x1
#define VFS_MODE_WRITE	0x2
#define VFS_MODE_APPEND	0x4

#define PT_VOID			0x00
#define PT_FAT12		0x01
#define PT_FAT16S		0x04
#define PT_EXTENDED		0x05
#define PT_FAT16B		0x06
#define PT_NTFS			0x07
#define PT_FAT32		0x0B
#define PT_FAT32LBA		0x0C
#define PT_FAT16BLBA	0x0E
#define PT_EXTENDEDLBA	0x0F
#define PT_OEM			0x12
#define PT_DYNAMIC		0x42
#define PT_SWAP			0x82
#define PT_NATIVE		0x83
#define PT_LVM			0x8E
#define PT_FBSD			0xA5
#define PT_OBSD			0XA6
#define PT_NBSD			0xA9
#define PT_LEGACY		0xEE
#define PT_EFI			0xEF

struct vfs_stream;

typedef enum{
	TYPE_DIR, TYPE_FILE, TYPE_MOUNT, TYPE_LINK, TYPE_DEV
}vfs_node_type_t;

typedef struct vfs_node{
		char *name;
		vfs_node_type_t type;
		struct vfs_node *parent;
		struct vfs_node *childs;	//Ungültig wenn kein TYPE_DIR oder TYPE_MOUNT. Bei TYPE_LINK -> Link zum Verknüpften Element
		struct vfs_node *next;
		union{
			vfs_device_t *dev;			//TYPE_DEVICE
			struct cdi_fs_filesystem *fs;	//TYPE_MOUNT
			size_t (*handler)(char *name, uint64_t start, size_t length, const void *buffer);
		};
		struct vfs_stream *stream;	//Stream, in dem die Node geöffnet ist
}vfs_node_t;

typedef struct vfs_stream{
	struct cdi_fs_stream stream;
	vfs_file_t id;
	vfs_mode_t mode;

	vfs_node_t *node;
	size_t ref_count;
}vfs_stream_t;

//Ein Stream vom Userspace hat eine ID, der auf einen Stream des Kernels gemappt ist
typedef struct{
	vfs_file_t id;
	vfs_file_t stream;
	vfs_mode_t mode;
}vfs_userspace_stream_t;

static vfs_node_t root;
static vfs_node_t *lastNode;
static uint8_t nextPartID = 0;
static list_t res_list;
static hashmap_t *streams = NULL;	//geöffnete Streams
static lock_t vfs_lock = LOCK_LOCKED;

static size_t getDirs(char ***Dirs, const char *Path)
{
	size_t i;
	char *tmp;
	//Erst Pfad sichern
	char *path_copy = strdup(Path);
	char *tmpPath = path_copy;

	if(!*Dirs)
		*Dirs = malloc(sizeof(char*));

	(*Dirs)[0] = strdup(strtok_s(&tmpPath, VFS_ROOT));
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

static void removeChilds(cdi_list_t childs)
{
	struct cdi_fs_res *res, *res2;
	size_t i = 0;
	while((res = cdi_list_get(childs, i++)))
	{
		removeChilds(res->children);
		size_t j = 0;
		while((res2 = list_get(res_list, j)))
		{
			if(res2 == res)
				list_remove(res_list, j);
			j++;
		}
	}
}

//TODO: Wenn der RAM voll ist werden weniger Ressourcen gecacht
//TODO: Wenn der RAM knapp wird kann die Speicherverwaltung das VFS auffordern den RAM ein bisschen frei zu machen
/*
 * Lädt wenn nötig eine Ressource
 * Parameter:	res = Ressource, die geladen werden soll
 * 				stream = Zu verwendenden Stream
 * Rückgabe:	false = Fehler / Ressource konnte nicht geladen werden
 * 				true = Ressource erfolgreich geladen
 */
static bool loadRes(struct cdi_fs_res *res, struct cdi_fs_stream *stream)
{
	struct cdi_fs_stream tmpStream = {
			.fs = stream->fs,
			.res = res
	};

	if(!res->loaded)
	{
		if(list_size(res_list) >= MAX_RES_BUFFER)
		{
			struct cdi_fs_res *tmpRes;
			size_t i = 0;
			while((tmpRes = list_get(res_list, i)) != NULL)
			{
				//Wenn die Ressource nicht geladen ist löschen wir sie einfach aus der Liste
				if(!tmpRes->loaded)
				{
					list_remove(res_list, i);
					break;
				}

				//Wenn die Ressource nirgends verwendet wird, können wir sie entladen
				if(tmpRes->stream_cnt <= 0)
				{
					struct cdi_fs_stream unload_stream = {
							.fs = stream->fs,
							.res = res
					};

					//Erst müssen wir alle Kinder noch von der Liste entfernen, da diese auch zerstört werden
					removeChilds(tmpRes->children);

					if(tmpRes->res->unload(&unload_stream))
					{
						list_remove(res_list, i);
						break;
					}
				}
				i++;
			}
			//Wenn keine Ressource freigegeben werden konnte, dann kann die neue Ressource nicht geladen werden
			if(i >= list_size(res_list))
				return false;
		}

		if(!res->res->load(&tmpStream))
			return false;
		list_push(res_list, res);
	}
	res->stream_cnt++;
	return true;
}

static void freeRes(struct cdi_fs_res *res)
{
	//Referenzzähler decrementieren
	do
	{
		res->stream_cnt -= (res->stream_cnt > 0) ? 1 : 0;
	}
	while ((res = res->parent) != NULL);
}

static struct cdi_fs_res *getRes(struct cdi_fs_stream *stream, const char *path)
{
	struct cdi_fs_res *res = NULL;
	struct cdi_fs_res *prevRes = stream->fs->root_res;

	char **dirs = NULL;
	size_t dirSize = getDirs(&dirs, path);

	if(!loadRes(prevRes, stream))
		return NULL;

	size_t i = 0;
	size_t j = 0;
	while(j < dirSize && (res = cdi_list_get(prevRes->children, i++)))
	{
		if(!strcmp(res->name, dirs[j]))
		{
			if(!loadRes(res, stream))
			{
				freeRes(res);
				return NULL;
			}
			prevRes = res;
			j++;
			i = 0;
		}
	}
	freeDirs(&dirs, dirSize);

	return res;
}

static uint64_t streamid_hash(const void *key, __attribute__((unused)) void *context)
{
	return (uint64_t)key;
}

static bool streamid_equal(const void *a, const void *b, __attribute__((unused)) void *context)
{
	return a == b;
}

static void vfs_stream_free(const void *s)
{
	vfs_stream_t *stream = (vfs_stream_t*)s;

	assert(stream != NULL);

	switch(stream->node->type)
	{
		case TYPE_MOUNT:
			freeRes(stream->stream.res);
		break;
	}
	free(stream);
}

static void vfs_userspace_stream_free(const void *s)
{
	vfs_userspace_stream_t *stream = (vfs_userspace_stream_t*)s;
	vfs_Close(stream->stream);
	free(stream);
}

static vfs_file_t getNextStreamID()
{
	static vfs_file_t nextFileID = 0;
	vfs_file_t id;
	do
	{
		id = __sync_fetch_and_add(&nextFileID, 1);
	}
	while(id == -1ul || LOCKED_RESULT(vfs_lock, hashmap_search(streams, (void*)id, NULL)));
	return id;
}

static vfs_file_t getNextUserspaceStreamID(process_t *p)
{
	vfs_file_t id = 0;
	while(id == -1ul || LOCKED_RESULT(vfs_lock, hashmap_search(p->streams, (void*)id, NULL)))
	{
		id++;
	}
	return id;
}

/*
 * Finde die letzte Node, die sich im Pfad befindet. Der Pfad muss absolut abgeben werden.
 * Parameter:	Path = Absoluter Pfad
 * 				remPath = restlicher Pfad
 */
static vfs_node_t *getLastNode(const char *Path, char **remPath)
{
	vfs_node_t *Node = &root;
	vfs_node_t *oldNode;
	char **Dirs = NULL;
	size_t NumDirs, i;

	//Welche Ordner?
	NumDirs = getDirs(&Dirs, Path);
	for(i = 0; i < NumDirs; i++)
	{
		oldNode = Node;
		Node = Node->childs;
		while(Node && strcmp(Node->name, Dirs[i]))
			Node = Node->next;
		if(!Node) break;
	}
	if(!Node)
	{
		Node = oldNode;
	}
	if(remPath != NULL)
	{
		*remPath = NULL;
		size_t size = 0;
		size_t j;
		for(j = 1; i < NumDirs; i++, j++)
		{
			size += strlen(Dirs[i]);
			*remPath = realloc(*remPath, size + j + 1);
			strcpy(*remPath + size - strlen(Dirs[i]) + j - 1, "/");
			strcpy(*remPath + size - strlen(Dirs[i]) + j, Dirs[i]);
		}
	}

	freeDirs(&Dirs, NumDirs);
	return Node;
}

/*
 * Finde die Node auf die der Pfad zeigt. Der Pfad muss absolut abgeben werden.
 * Parameter:	Path = Absoluter Pfad
 */
static vfs_node_t *getNode(const char *Path)
{
	vfs_node_t *Node = &root;
	char **Dirs = NULL;
	size_t NumDirs, i;

	//Welche Ordner?
	NumDirs = getDirs(&Dirs, Path);
	for(i = 0; i < NumDirs; i++)
	{
		Node = Node->childs;
		while(Node && strcmp(Node->name, Dirs[i]))
			Node = Node->next;
		if(!Node) return NULL;
	}

	freeDirs(&Dirs, NumDirs);
	return Node;
}

void vfs_Init(void)
{
	res_list = list_create();
	streams = hashmap_create(streamid_hash, streamid_hash, streamid_equal, NULL, vfs_stream_free, NULL, 3);
	assert(streams != NULL);

	//Root
	root.name = VFS_ROOT;
	root.next = NULL;
	root.childs = NULL;
	root.parent = &root;
	root.type = TYPE_DIR;

	//Virtuelle Ordner anlegen
	vfs_node_t *Node;
	//Unterordner "dev" anlegen: für Gerätedateien
	Node = calloc(1, sizeof(vfs_node_t));
	Node->next = NULL;
	Node->parent = &root;
	Node->childs = NULL;
	Node->name = "dev";
	Node->type = TYPE_DIR;	//Mount->fs wird nicht benötigt
	root.childs = Node;

	//Unterordner "sysinf" anlegen: für Systeminformationen
	Node->next = calloc(1, sizeof(vfs_node_t));
	Node = Node->next;
	Node->childs = NULL;
	Node->next = NULL;
	Node->parent = &root;
	Node->name = "sysinf";
	Node->type = TYPE_DIR;

	//Unterordner "mount" anlegen: für Mountpoints
	Node->next = calloc(1, sizeof(vfs_node_t));
	Node = Node->next;
	Node->childs = NULL;
	Node->next = NULL;
	Node->parent = &root;
	Node->name = "mount";
	Node->type = TYPE_DIR;

	lastNode = Node;
	unlock(&vfs_lock);
}

/*
 * Eine Datei öffnen
 * Parameter:	path = Pfad zur Datei
 * 				mode = Modus, in der die Datei geöffnet werden soll
 */
vfs_file_t vfs_Open(const char *path, vfs_mode_t mode)
{
	if(path == NULL || strlen(path) == 0 || (!mode.read && !mode.write))
		return -1;

	char *remPath;
	vfs_stream_t *stream;
	vfs_node_t *node = getLastNode(path, &remPath);
	stream = calloc(1, sizeof(*stream));
	stream->id = getNextStreamID();
	stream->mode = mode;

	stream->node = node;
	stream->ref_count = 1;
	switch(node->type)
	{
		case TYPE_MOUNT:
			stream->stream.fs = node->fs;
			stream->stream.res = getRes(&stream->stream, remPath);
			if(stream->stream.res == NULL)
			{
				free(stream);
				if(remPath)
					free(remPath);
				return -1;
			}
		break;
		case TYPE_DIR:	//Ordner kann man nicht öffnen
			printf("Versucht Ordner zu öffnen!\n");
			free(stream);
			if(remPath)
				free(remPath);
			return -1;
		break;
		case TYPE_DEV:
			mode.empty = false;
			mode.append = false;
			mode.create = false;
		break;
	}

	if(mode.empty)
	{
		if(stream->stream.fs->read_only || !stream->stream.res->flags.write)
		{
			stream->stream.res->res->unload(&stream->stream);
			free(stream);
			return -1;
		}
		stream->stream.res->file->truncate(&stream->stream, 0);
	}
	if(remPath)
		free(remPath);

	//In Hashtable einfügen
	LOCKED_TASK(vfs_lock, hashmap_set(streams, (void*)stream->id, stream));

	assert(LOCKED_RESULT(vfs_lock, hashmap_search(streams, (void*)stream->id, NULL)));

	return stream->id;
}

vfs_file_t vfs_Reopen(const vfs_file_t streamid, vfs_mode_t mode)
{
	vfs_stream_t *stream;

	if((!mode.read && !mode.write))
		return -1;

	assert(streams != NULL);
	if(!LOCKED_RESULT(vfs_lock, hashmap_search(streams, (void*)streamid, (void**)&stream)))
		return -1;

	if(memcmp(&mode, &stream->mode, sizeof(vfs_mode_t)) != 0)
	{
		//Klone den Stream mit dem entsprechendem Modus
		vfs_stream_t *new_stream = malloc(sizeof(vfs_stream_t));
		if(new_stream == NULL)
			return -1;

		memcpy(new_stream, stream, sizeof(vfs_stream_t));

		new_stream->id = getNextStreamID();
		new_stream->ref_count = 1;
		new_stream->stream.res->stream_cnt++;

		stream = new_stream;

		LOCKED_TASK(vfs_lock, hashmap_set(streams, (void*)new_stream->id, new_stream));
	}

	assert(LOCKED_RESULT(vfs_lock, hashmap_search(streams, (void*)stream->id, NULL)));

	return stream->id;
}

/*
 * Eine Datei schliessen
 * Parameter:	stream = Stream der geschlossen werden soll
 *///TODO: Prüfen, ob der Stream noch in verwendung ist und erst dann löschen
void vfs_Close(vfs_file_t streamid)
{
	LOCKED_TASK(vfs_lock, hashmap_delete(streams, (void*)streamid));
	assert(!LOCKED_RESULT(vfs_lock, hashmap_search(streams, (void*)streamid, NULL)));
}

/*
 * Eine Datei lesen
 * Parameter:	Path = Pfad zur Datei als String
 * 				start = Anfangsbyte, an dem angefangen werden soll zu lesen
 * 				length = Anzahl der Bytes, die gelesen werden sollen
 * 				Buffer = Buffer in den die Bytes geschrieben werden
 */
size_t vfs_Read(vfs_file_t streamid, uint64_t start, size_t length, void *buffer)
{
	vfs_stream_t *stream;

	if(buffer == NULL)
		return 0;

	assert(streams != NULL);
	if(!LOCKED_RESULT(vfs_lock, hashmap_search(streams, (void*)streamid, (void**)&stream)))
		return 0;

	size_t sizeRead = 0;

	//Erst überprüfen ob der Stream zum lesen geöffnet wurde
	if(!stream->mode.read)
		return 0;

	vfs_node_t *node = (stream->node->type == TYPE_LINK) ? stream->node->childs : stream->node;

	switch(node->type)
	{
		case TYPE_DEV:
			if(stream->node->dev->read != NULL)
				sizeRead = stream->node->dev->read(stream->node->dev->opaque, start, length, buffer);
		break;
		case TYPE_MOUNT:
			if(stream->stream.res->flags.read)
				sizeRead = stream->stream.res->file->read(&stream->stream, start, length, buffer);
		break;
	}
	return sizeRead;
}

size_t vfs_Write(vfs_file_t streamid, uint64_t start, size_t length, const void *buffer)
{
	vfs_stream_t *stream;

	if(buffer == NULL)
		return 0;

	 if(!LOCKED_RESULT(vfs_lock, hashmap_search(streams, (void*)streamid, (void**)&stream)))
		 return 0;

	size_t sizeWritten = 0;

	//Erst überprüfen ob der Stream zum schreiben geöffnet wurde
	if(!stream->mode.write)
		return 0;

	vfs_node_t *node = (stream->node->type == TYPE_LINK) ? stream->node->childs : stream->node;

	switch(node->type)
	{
		case TYPE_DEV:
			if(stream->node->dev->write != NULL)
				sizeWritten = stream->node->dev->write(stream->node->dev->opaque, start, length, buffer);
		break;
		case TYPE_MOUNT:
			//Überprüfen, ob auf das Dateisystem geschrieben werden darf
			if(!stream->stream.fs->read_only && stream->stream.res->flags.write)
				sizeWritten = stream->stream.res->file->read(&stream->stream, start, length, buffer);
		break;
		case TYPE_FILE:
			//Wenn ein Handler gesetzt ist, dann Handler aufrufen
			if(stream->node->handler != NULL)
				sizeWritten = stream->node->handler(stream->node->name, start, length, buffer);
		break;
	}
	return sizeWritten;
}

//TODO: Erbe alle geöffneten Stream vom Vaterprozess
int vfs_initUserspace(process_t *parent, process_t *p, const char *stdin, const char *stdout, const char *stderr)
{
	assert(p != NULL && ((parent == NULL && stdin != NULL && stdout != NULL && stderr != NULL) || parent != NULL));
	if((p->streams = hashmap_create(streamid_hash, streamid_hash, streamid_equal, NULL, vfs_userspace_stream_free, NULL, 3)) == NULL)
		return 1;

	vfs_file_t streamid;
	vfs_userspace_stream_t *stream;
	vfs_mode_t m = {
			.read = true
	};
	if(parent != NULL && stdin == NULL)
	{
		LOCKED_TASK(parent->lock, hashmap_search(parent->streams, (void*)0, (void**)&stream));
		assert(stream != NULL);
		streamid = vfs_Reopen(stream->stream, m);
	}
	else
	{
		streamid = vfs_Open(stdin, m);
	}
	if(streamid == -1)
		return 0;
	stream = malloc(sizeof(vfs_userspace_stream_t));
	if(stream == NULL)
		return 0;
	stream->id = 0;
	stream->mode = m;
	stream->stream = streamid;
	hashmap_set(p->streams, (void*)0, stream);

	m.write = true;
	m.read = false;
	if(parent != NULL && stdout == NULL)
	{
		LOCKED_TASK(parent->lock, hashmap_search(parent->streams, (void*)1, (void**)&stream));
		assert(stream != NULL);
		streamid = vfs_Reopen(stream->stream, m);
	}
	else
	{
		streamid = vfs_Open(stdout, m);
	}
	if(streamid == -1)
		return 0;
	stream = malloc(sizeof(vfs_userspace_stream_t));
	if(stream == NULL)
		return 0;
	stream->id = 1;
	stream->mode = m;
	stream->stream = streamid;
	hashmap_set(p->streams, (void*)1, stream);

	if(parent != NULL && stderr == NULL)
	{
		LOCKED_TASK(parent->lock, hashmap_search(parent->streams, (void*)2, (void**)&stream));
		assert(stream != NULL);
		streamid = vfs_Reopen(stream->stream, m);
	}
	else
	{
		streamid = vfs_Open(stderr, m);
	}
	if(streamid == -1)
		return 0;
	stream = malloc(sizeof(vfs_userspace_stream_t));
	if(stream == NULL)
		return 0;
	stream->id = 2;
	stream->mode = m;
	stream->stream = streamid;
	hashmap_set(p->streams, (void*)2, stream);
	return 1;
}

vfs_node_t *vfs_createNode(const char *path, const char *name, vfs_node_type_t type, void *data)
{
	vfs_node_t *child;
	char *rempath = NULL;

	vfs_node_t *node = getLastNode(path, &rempath);

	if(node->type != TYPE_DIR)
	{
		free(rempath);
		return NULL;
	}

	child = calloc(1, sizeof(vfs_node_t));
	child->name = strdup(name);
	child->parent = node;
	child->next = node->childs;
	node->childs = child;
	child->type = type;

	switch(type)
	{
		case TYPE_DEV:
			child->dev = data;
		break;
		case TYPE_FILE:
			child->handler = data;
		break;
		case TYPE_LINK:
			child->childs = data;
		break;
		case TYPE_MOUNT:
			child->fs = data;
		break;
	}

	free(rempath);

	return child;
}

/*
 * Gibt die Metainformationen einer Datei zurück
 * Parameter:	stream = stream dessen Grösse abgefragt wird (muss eine Datei sein)
 * Rückgabe:	Grösse des Streams oder 0 bei Fehler
 */
uint64_t vfs_getFileinfo(vfs_file_t streamid, vfs_fileinfo_t info)
{
	vfs_stream_t *stream;

	if(!LOCKED_RESULT(vfs_lock, hashmap_search(streams, (void*)streamid, (void**)&stream)))
		return 0;

	if(stream->node->type == TYPE_MOUNT && stream->stream.res->file != NULL)
	{
		switch(info)
		{
			case VFS_INFO_FILESIZE:
				return stream->stream.res->res->meta_read(&stream->stream, CDI_FS_META_SIZE);
			break;
			case VFS_INFO_USEDBLOCKS:
				return stream->stream.res->res->meta_read(&stream->stream, CDI_FS_META_USEDBLOCKS);
			break;
			case VFS_INFO_BLOCKSIZE:
				return stream->stream.res->res->meta_read(&stream->stream, CDI_FS_META_BLOCKSZ);
			break;
			case VFS_INFO_CREATETIME:
				return stream->stream.res->res->meta_read(&stream->stream, CDI_FS_META_CREATETIME);
			break;
			case VFS_INFO_ACCESSTIME:
				return stream->stream.res->res->meta_read(&stream->stream, CDI_FS_META_ACCESSTIME);
			break;
			case VFS_INFO_CHANGETIME:
				return stream->stream.res->res->meta_read(&stream->stream, CDI_FS_META_CHANGETIME);
			break;
		}
	}

	return 0;
}


/*
 * Mountet ein Dateisystem (fs) an den entsprechenden Mountpoint (Mount)
 * Parameter:	Mount = Mountpoint (Pfad)
 * 				Dev = Pfad zum Gerät
 * Rückgabe:	!0 bei Fehler
 */
int vfs_Mount(const char *Mountpath, const char *Dev)
{
	vfs_node_t *mount;
	vfs_node_t *devNode;
	if((devNode = getNode(Dev)) == NULL)
		return 1;

	if((mount = getNode(Mountpath)) == NULL)
		return 2;

	if(devNode->type != TYPE_DEV)
		return 3;

	if(devNode->dev->getValue == NULL || strcmp(VFS_DEVICE_PARTITION, (char*)devNode->dev->getValue(devNode->dev->opaque, FUNC_TYPE)) != 0)
		return 7;

	struct cdi_fs_filesystem *fs = devNode->dev->getValue(devNode->dev->opaque, FUNC_DATA);
	if(fs == NULL)
		return 5;

	vfs_node_t *new = calloc(1, sizeof(vfs_node_t));
	asprintf(&new->name, "%u", nextPartID++);
	new->type = TYPE_MOUNT;
	new->parent = mount;
	new->next = mount->childs;
	mount->childs = new;
	new->fs = fs;
	//Dateisystem initialisieren
	if(!fs->driver->fs_init(fs))
		return 4;

	return 0;
}

/*
 * Unmountet ein Dateisystem am entsprechenden Mountpoint (Mount)
 * Parameter:	Mount = Mountpoint (Pfad)
 * Rückgabe:	!0 bei Fehler
 */
int vfs_Unmount(const char *Mount)
{
	vfs_node_t *mount = getNode(Mount);
	if(!mount || mount->type != TYPE_MOUNT)
		return 1;

	vfs_node_t *parentNode;
	parentNode = mount->parent;
	if(parentNode->childs == mount)
		parentNode->childs = mount->next;
	else
		parentNode->childs->next = mount->next;

	//FS deinitialisieren
	mount->fs->driver->fs_destroy(mount->fs);

	free(mount->name);
	free(mount);

	return 0;
}

/*
 * Mountet das erste Gerät
 * Rückgabe:	!0 bei Fehler
 */
int vfs_MountRoot(void)
{
	char *DevPath;
	int status = -1;
	vfs_node_t *dev = getNode("dev");
	vfs_node_t *node = dev->childs;
	do
	{
		if(node->type == TYPE_DEV)
		{
			nextPartID = 0;
			asprintf(&DevPath, "dev/%s", node->name);
			status = vfs_Mount("/mount", DevPath);
			free(DevPath);
			if(status == 0)
			{
				FILE *fp = fopen("/mount/0/kernel", "r");
				if(fp != NULL)
				{
					fclose(fp);
					break;
				}
				vfs_Unmount("mount/0");
			}
		}
	}
	while((node = node->next) != NULL);
	return status;
}

/*
 * Unmountet root
 * Rückgabe:	!0 bei Fehler
 */
int vfs_UnmountRoot(void)
{
	return vfs_Unmount("/mount/0");
}

/*
 * Splittet den Pfad in Dateinamen und Pfad auf
 * Parameter:	path = Pfad
 * 				file = Name der Datei
 * 				dev = Name der Partition
 * Rückgabe:	NULL bei Fehler oder den Pfad
 */
char *splitPath(const char *path, char **file, char **dev)
{
	if(strlen(path) == 0)
		return NULL;

	char *tmp = strrchr(path, VFS_SEPARATOR);
	if(tmp == NULL)
		return NULL;
	*tmp = '\0';
	tmp++;
	if(*tmp == '\0')
		*file = NULL;
	else
		*file = tmp;

	tmp = strchr(path, ':');
	if(tmp == NULL)
		return NULL;
	*tmp = '\0';
	*dev = path;

	return path + strlen(*dev) + 1;
}

/*
 * Registriert ein Gerät. Dazu wird eine Gerätedatei im Verzeichniss /dev angelegt.
 * Parameter:	dev = Gerätestruktur
 */
void vfs_RegisterDevice(vfs_device_t *dev)
{
	const char *Path = "/dev";	//Pfad zu den Gerätendateien
	vfs_node_t *Node, *tmp;
	//ist der Ordner schon vorhanden?
	if(!(tmp = getNode(Path))) return;	//Fehler

	//Gerätedatei anlegen
	//wird vorne angelegt
	Node = calloc(1, sizeof(*Node));
	Node->type = TYPE_DEV;
	Node->dev = dev;
	Node->next = tmp->childs;
	tmp->childs = Node;
	Node->name = strdup(dev->getValue(dev->opaque, FUNC_NAME));
	Node->parent = tmp;
	{
	}
}

//Syscalls
//TODO: Define errors correctly via macros
vfs_file_t vfs_syscall_open(const char *path, vfs_mode_t mode)
{
	assert(currentProcess != NULL);
	vfs_userspace_stream_t *stream = malloc(sizeof(vfs_userspace_stream_t));
	if(stream == NULL)
		return -1;
	vfs_file_t streamid = vfs_Open(path, mode);
	if(streamid == -1)
	{
		free(stream);
		return -1;
	}
	stream->id = getNextUserspaceStreamID(currentProcess);
	stream->mode = mode;
	stream->stream = streamid;
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
	if(!stream->mode.read)
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
	if(!stream->mode.write)
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
#endif
