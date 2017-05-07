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
#include "ctype.h"

#define MAX_RES_BUFFER	100		//Anzahl an Ressourcen, die maximal geladen werden. Wenn der Buffer voll ist werden nicht benötigte Ressourcen überschrieben

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
			vfs_filesystem_t *fs;		//TYPE_MOUNT
			size_t (*handler)(char *name, uint64_t start, size_t length, const void *buffer);
		};
		struct vfs_stream *stream;	//Stream, in dem die Node geöffnet ist
}vfs_node_t;

typedef struct vfs_stream{
	struct cdi_fs_stream stream;
	vfs_file_t id;
	vfs_mode_t mode;

	vfs_node_t *node;
	REFCOUNT_FIELD;
}vfs_stream_t;

//Ein Stream vom Userspace hat eine ID, der auf einen Stream des Kernels gemappt ist
typedef struct{
	vfs_file_t id;
	vfs_file_t stream;
	vfs_mode_t mode;
}vfs_userspace_stream_t;

static vfs_node_t root;
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

	if(!loadRes(prevRes, stream))
		return NULL;

	if(path == NULL)
		return prevRes;

	char **dirs = NULL;
	size_t dirSize = getDirs(&dirs, path);

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

/*
 * Wird für die Hashtable verwendet. Darf nicht auf vfs_lock locken.
 */
static void vfs_stream_free(const void *s)
{
	vfs_stream_t *stream = (vfs_stream_t*)s;

	assert(stream != NULL);

	switch(stream->node->type)
	{
		case TYPE_MOUNT:
			freeRes(stream->stream.res);
		break;
		default:
			//Nichts machen
		break;
	}
	free(stream);
}

/*
 * Wird aufgerufen, wenn ein Stream keine Verweise mehr hat.
 */
static void vfs_stream_closed(const void *s)
{
	const vfs_stream_t *stream = (const vfs_stream_t*)s;
	vfs_file_t streamid = stream->id;

	//Stream wird nicht mehr verwendet
	LOCKED_TASK(vfs_lock, hashmap_delete(streams, (void*)streamid));
	assert(!LOCKED_RESULT(vfs_lock, hashmap_search(streams, (void*)streamid, NULL)));
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
	vfs_node_t *oldNode = NULL;	//Ohne Initialisierung meckert der Compiler
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

/*
 * Erstelle eine simple Node. Setzt nicht den Typ der Node und nicht die abhängigen Daten.
 * Parameter:	parent = Node zu welcher die neue Node hinzugefügt werden soll
 * 				name = Name der Node
 * Rückgabe:	Pointer zur neuen Node
 */
static vfs_node_t *createBaseNode(vfs_node_t *parent, const char *name)
{
	vfs_node_t *node;

	if(parent == NULL || parent->type != TYPE_DIR)
		return NULL;

	node = calloc(1, sizeof(vfs_node_t));
	if(node == NULL)
		return NULL;

	node->name = strdup(name);
	node->parent = parent;
	node->next = parent->childs;
	parent->childs = node;

	return node;
}

/*
 * Löscht eine Node. Löscht keine Kinder.
 * Parameter:	node = Node die gelöscht werden soll
 */
static void deleteNode(vfs_node_t *node)
{
	if(node != NULL)
	{
		assert(node != &root);	//Rootnode darf nicht gelöscht werden

		lock(&vfs_lock);
		vfs_node_t *parentNode;
		parentNode = node->parent;
		if(parentNode->childs == node)
			parentNode->childs = node->next;
		else
		{
			vfs_node_t *tmp;
			for(tmp = parentNode->childs; tmp != NULL; tmp = tmp->next)
			{
				if(tmp->next == node)
				{
					tmp->next = node->next;
					break;
				}
			}
			if(tmp == NULL)
			{
				unlock(&vfs_lock);
				return;
			}
		}
		unlock(&vfs_lock);

		free(node->name);
		free(node);
	}
}

/*
 * Erstelle eine Gerätenode.
 * Parameter:	parent = Node zu welcher die neue Node hinzugefügt werden soll
 * 				name = Name der Node
 * Rückgabe:	Pointer zur neuen Node
 */
static vfs_node_t *createDirNode(vfs_node_t *parent, const char *name)
{
	lock(&vfs_lock);

	vfs_node_t *node = createBaseNode(parent, name);

	if(node == NULL)
	{
		unlock(&vfs_lock);
		return NULL;
	}

	node->type = TYPE_DIR;

	unlock(&vfs_lock);

	return node;
}

/*
 * Erstelle eine Dateinode.
 * Parameter:	parent = Node zu welcher die neue Node hinzugefügt werden soll
 * 				name = Name der Node
 * 				handler = Handler für die Dateioperation
 * Rückgabe:	Pointer zur neuen Node
 */
static vfs_node_t *createFileNode(vfs_node_t *parent, const char *name, size_t (*handler)(char*, uint64_t, size_t, const void*))
{
	lock(&vfs_lock);

	vfs_node_t *node = createBaseNode(parent, name);

	if(node == NULL)
	{
		unlock(&vfs_lock);
		return NULL;
	}

	node->type = TYPE_FILE;
	node->handler = handler;

	unlock(&vfs_lock);

	return node;
}

/*
 * Erstelle eine Mountnode.
 * Parameter:	parent = Node zu welcher die neue Node hinzugefügt werden soll
 * 				name = Name der Node
 * 				fs = Dateisystem, welches gemounted wurde
 * Rückgabe:	Pointer zur neuen Node
 */
static vfs_node_t *createMountNode(vfs_node_t *parent, const char *name, vfs_filesystem_t *fs)
{
	lock(&vfs_lock);

	vfs_node_t *node = createBaseNode(parent, name);

	if(node == NULL)
	{
		unlock(&vfs_lock);
		return NULL;
	}

	node->type = TYPE_MOUNT;
	node->fs = fs;

	unlock(&vfs_lock);

	return node;
}

/*
 * Erstelle eine Linknode.
 * Parameter:	parent = Node zu welcher die neue Node hinzugefügt werden soll
 * 				name = Name der Node
 * 				link = Node zu der verlinkt wird
 * Rückgabe:	Pointer zur neuen Node
 */
static vfs_node_t *createLinkNode(vfs_node_t *parent, const char *name, vfs_node_t *link)
{
	lock(&vfs_lock);

	vfs_node_t *node = createBaseNode(parent, name);

	if(node == NULL)
	{
		unlock(&vfs_lock);
		return NULL;
	}

	node->type = TYPE_LINK;
	node->childs = link;

	unlock(&vfs_lock);

	return node;
}

/*
 * Erstelle einen Gerätenode.
 * Parameter:	parent = Node zu welcher die neue Node hinzugefügt werden soll
 * 				name = Name der Node
 * 				device = Gerät für die die Node erstellt werden soll
 * Rückgabe:	Pointer zur neuen Node
 */
static vfs_node_t *createDeviceNode(vfs_node_t *parent, const char *name, vfs_device_t *device)
{
	lock(&vfs_lock);

	vfs_node_t *node = createBaseNode(parent, name);

	if(node == NULL)
	{
		unlock(&vfs_lock);
		return NULL;
	}

	node->type = TYPE_DEV;
	node->dev = device;

	unlock(&vfs_lock);

	return node;
}

/*
 * Gibt den verlinkten Knoten zurück
 * Parameter:	node = Startknoten
 * Rückgabe:	Pointer zur verlinkten Node
 */
static vfs_node_t *resolveLink(vfs_node_t *node)
{
	while(node->type == TYPE_LINK)
	{
		node = node->childs;
		assert(node != NULL);
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

	unlock(&vfs_lock);

	//Virtuelle Ordner anlegen
	//Unterordner "dev" anlegen: für Gerätedateien
	createDirNode(&root, "dev");

	//Unterordner "sysinf" anlegen: für Systeminformationen
	createDirNode(&root, "sysinf");

	//Unterordner "mount" anlegen: für Mountpoints
	createDirNode(&root, "mount");
}

/*
 * Eine Datei öffnen
 * Parameter:	path = Pfad zur Datei
 * 				mode = Modus, in der die Datei geöffnet werden soll
 */
vfs_file_t vfs_Open(const char *path, vfs_mode_t mode)
{
	if(path == NULL || strlen(path) == 0 || !check_path(path) || (!mode.read && !mode.write) || (mode.write && mode.directory))
		return -1;

	char *remPath;
	vfs_stream_t *stream;
	vfs_node_t *node = getLastNode(path, &remPath);
	stream = calloc(1, sizeof(*stream));
	stream->id = getNextStreamID();
	stream->mode = mode;

	stream->node = resolveLink(node);
	REFCOUNT_INIT(stream, vfs_stream_closed);
	switch(node->type)
	{
		case TYPE_MOUNT:
			stream->stream.fs = &node->fs->fs;
			stream->stream.res = getRes(&stream->stream, remPath);
			//Löse symlinks auf
			while(stream->stream.res != NULL && stream->stream.res->link != NULL)
			{
				const char *link_path = stream->stream.res->link->read_link(&stream->stream);
				stream->stream.res = getRes(&stream->stream, link_path);
			}

			if(stream->stream.res == NULL || (mode.directory && stream->stream.res->dir == NULL))
			{
				free(stream);
				if(remPath)
					free(remPath);
				return -1;
			}
		break;
		case TYPE_DIR:
			if(!mode.directory || remPath != NULL)
			{
				free(stream);
				if(remPath)
					free(remPath);
				return -1;
			}
			assert(remPath == NULL);
		break;
		case TYPE_DEV:
			if(mode.directory)
			{
				free(stream);
				if(remPath)
					free(remPath);
				return -1;
			}
			stream->mode.empty = false;
			stream->mode.append = false;
			stream->mode.create = false;
			assert(remPath == NULL);
		break;
		case TYPE_LINK:
			assert(false);
		break;
		case TYPE_FILE:
			//Nichts zu tun
		break;
	}

	if(stream->mode.empty)
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

	//Stream reservieren
	stream = REFCOUNT_RETAIN(stream);
	if(stream == NULL)
		return -1;

	if(memcmp(&mode, &stream->mode, sizeof(vfs_mode_t)) != 0)
	{
		//Klone den Stream mit dem entsprechendem Modus
		vfs_stream_t *new_stream = malloc(sizeof(vfs_stream_t));
		if(new_stream == NULL)
			return -1;

		memcpy(new_stream, stream, sizeof(vfs_stream_t));

		new_stream->id = getNextStreamID();
		REFCOUNT_INIT(new_stream, vfs_stream_closed);
		__sync_fetch_and_add(&new_stream->stream.res->stream_cnt, 1);

		//Reservierten Stream wieder freigeben
		REFCOUNT_RELEASE(stream);

		stream = new_stream;

		LOCKED_TASK(vfs_lock, hashmap_set(streams, (void*)new_stream->id, new_stream));
	}

	assert(LOCKED_RESULT(vfs_lock, hashmap_search(streams, (void*)stream->id, NULL)));

	return stream->id;
}

/*
 * Eine Datei schliessen
 * Parameter:	stream = Stream der geschlossen werden soll
 */
void vfs_Close(vfs_file_t streamid)
{
	vfs_stream_t *stream;
	if(!LOCKED_RESULT(vfs_lock, hashmap_search(streams, (void*)streamid, (void**)&stream)))
		return;

	//Reservierten Stream freigeben
	REFCOUNT_RELEASE(stream);
}

static size_t readNodeChilds(vfs_node_t *node, uint64_t start, size_t size, vfs_userspace_direntry_t *buffer)
{
	assert(node != NULL);
	assert(node->type == TYPE_DIR || node->type == TYPE_MOUNT);
	size_t sizeRead = 0;
	vfs_node_t *tmp = node->childs;
	size_t i = 0;
	//Überspringe die nicht benötigten Einträge
	while(tmp != NULL && i < start)
	{
		tmp = tmp->next;
		i++;
	}
	while(tmp != NULL)
	{
		size_t name_length = strlen(tmp->name);
		size_t entry_size = sizeof(vfs_userspace_direntry_t) + name_length + 1;
		if(sizeRead + entry_size > size)
			break;
		vfs_userspace_direntry_t *entry = (vfs_userspace_direntry_t*)((char*)buffer + sizeRead);
		entry->size = entry_size;

		switch(tmp->type)
		{
			case TYPE_DIR:
			case TYPE_MOUNT:
				entry->type = UDT_DIR;
			break;
			case TYPE_FILE:
				entry->type = UDT_FILE;
			break;
			case TYPE_LINK:
				entry->type = UDT_LINK;
			break;
			case TYPE_DEV:
				entry->type = UDT_DEV;
			break;
			default:
				entry->type = UDT_UNKNOWN;
			break;
		}

		strcpy((char*)&entry->name, tmp->name);
		sizeRead += entry_size;
		tmp = tmp->next;
	}
	return sizeRead;
}

/*
 * Einträge aus einem Ordner lesen
 * Parameter:	streamid = Id des Streams, der den Ordner repräsentiert
 * 				start = Index des zu startenden Ordnereintrags
 * 				size = Grösse des Buffers
 * 				buffer = Buffer in die die Einträge geschrieben werden
 * Rückgabe:	Anzahl Bytes, die geschrieben wurden
 */
static size_t ReadDir(vfs_stream_t *stream, uint64_t start, size_t size, vfs_userspace_direntry_t *buffer)
{
	assert(buffer != NULL);
	assert(streams != NULL);

	size_t sizeRead = 0;

	//Erst überprüfen ob der Stream zum lesen geöffnet wurde und einen Ordner repräsentiert
	if(!stream->mode.read || !stream->mode.directory)
		return 0;

	vfs_node_t *node = stream->node;

	switch(node->type)
	{
		case TYPE_MOUNT:
			//FIXME: hack
			if(stream->stream.res == node->fs->fs.root_res)
				sizeRead = readNodeChilds(node, start, size, buffer);
			if(sizeRead < size)
			{
				cdi_list_t childs;
				struct cdi_fs_res *child_res;
				vfs_userspace_direntry_t *entry;
				childs = stream->stream.res->dir->list(&stream->stream);
				if(childs == NULL || cdi_list_size(childs) == 0)
					return 0;
				size_t i = start;
				while((child_res = cdi_list_get(childs, i++)))
				{
					size_t name_length = strlen(child_res->name);
					size_t entry_size = sizeof(vfs_userspace_direntry_t) + name_length + 1;
					if(sizeRead + entry_size > size)
						break;
					entry = (vfs_userspace_direntry_t*)((char*)buffer + sizeRead);
					entry->size = entry_size;

					if(child_res->dir != NULL)
						entry->type = UDT_DIR;
					else if(child_res->file != NULL)
						entry->type = UDT_FILE;
					else if(child_res->link != NULL)
						entry->type = UDT_LINK;
					else
						entry->type = UDT_UNKNOWN;

					strcpy((char*)&entry->name, child_res->name);
					sizeRead += entry_size;
				}
			}
		break;
		case TYPE_DIR:
			sizeRead = readNodeChilds(node, start, size, buffer);
		break;
		default:
			assert(false);
		break;
	}

	return sizeRead;
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

	vfs_node_t *node = stream->node;

	//Wenn der Stream einen Ordner repräsentiert rufen wir die entsprechende Funktion auf
	if(stream->mode.directory)
		return ReadDir(stream, start, length, buffer);

	switch(node->type)
	{
		case TYPE_DEV:
			if(stream->node->dev->read != NULL)
				sizeRead = stream->node->dev->read(stream->node->dev->opaque, start, length, buffer);
		break;
		case TYPE_MOUNT:
			if(stream->stream.res->flags.read)
			{
				size_t filesize = stream->stream.res->res->meta_read(&stream->stream, CDI_FS_META_SIZE);
				if(start > filesize)
					start = filesize;
				if(start + length > filesize)
					length = filesize - start;
				sizeRead = stream->stream.res->file->read(&stream->stream, start, length, buffer);
			}
		break;
		default:
			assert(false);
		break;
	}
	assert(sizeRead <= length);
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

	vfs_node_t *node = stream->node;

	switch(node->type)
	{
		case TYPE_DEV:
			if(stream->node->dev->write != NULL)
				sizeWritten = stream->node->dev->write(stream->node->dev->opaque, start, length, buffer);
		break;
		case TYPE_MOUNT:
			//Überprüfen, ob auf das Dateisystem geschrieben werden darf
			if(!stream->stream.fs->read_only && stream->stream.res->flags.write)
				sizeWritten = stream->stream.res->file->write(&stream->stream, start, length, buffer);
		break;
		case TYPE_FILE:
			//Wenn ein Handler gesetzt ist, dann Handler aufrufen
			if(stream->node->handler != NULL)
				sizeWritten = stream->node->handler(stream->node->name, start, length, buffer);
		break;
		default:
			assert(false);
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
	if(streamid == -1ul)
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
	if(streamid == -1ul)
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
	if(streamid == -1ul)
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

void vfs_deinitUserspace(process_t *p)
{
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
	else if(stream->node->type == TYPE_DEV)
	{
		switch(info)
		{
			case VFS_INFO_BLOCKSIZE:
				if(stream->node->dev->getCapabilities(stream->node->dev->opaque) & VFS_DEV_CAP_BLOCKSIZE)
					return (uint64_t)stream->node->dev->function(stream->node->dev->opaque, VFS_DEV_FUNC_BLOCKSIZE);
				else
					return 0;
			break;
			default:
				return 0;
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

	if((vfs_device_type_t)devNode->dev->function(devNode->dev->opaque, VFS_DEV_FUNC_TYPE) != VFS_DEVICE_PARTITION)
		return 7;

	vfs_filesystem_t *fs = devNode->dev->function(devNode->dev->opaque, VFS_DEV_FUNC_MOUNT);
	if(fs == NULL)
		return 5;

	if(mount->type == TYPE_MOUNT)
		return 6;

	mount->fs = fs;
	mount->type = TYPE_MOUNT;

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

	mount->fs->device->function(mount->fs->device->opaque, VFS_DEV_FUNC_UMOUNT);

	mount->type = TYPE_DIR;

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
			asprintf(&DevPath, "dev/%s", node->name);
			status = vfs_Mount(VFS_ROOT, DevPath);
			free(DevPath);
			if(status == 0)
			{
				FILE *fp = fopen("/kernel", "r");
				if(fp != NULL)
				{
					fclose(fp);
					break;
				}
				vfs_Unmount(VFS_ROOT);
			}
		}
	}
	while((node = node->next) != NULL);
	return status;
}

/*
 * Registriert ein Gerät. Dazu wird eine Gerätedatei im Verzeichniss /dev angelegt.
 * Parameter:	dev = Gerätestruktur
 */
void vfs_RegisterDevice(vfs_device_t *dev)
{
	const char *Path = "/dev";	//Pfad zu den Gerätendateien
	vfs_node_t *tmp;

	assert(dev != NULL);
	assert(dev->function != NULL);
	assert(dev->getCapabilities != NULL);

	//ist der Ordner schon vorhanden?
	if(!(tmp = getNode(Path))) return;	//Fehler

	//Gerätedatei anlegen
	vfs_node_t *dev_node = createDeviceNode(tmp, dev->function(dev->opaque, VFS_DEV_FUNC_NAME), dev);
	if(dev_node != NULL && (dev->getCapabilities(dev->opaque) & VFS_DEV_CAP_PARTITIONS))
	{
		char *dev_path = malloc(strlen(Path) + strlen(dev_node->name) + 2);
		sprintf(dev_path, "%s/%s", Path, dev_node->name);
		vfs_mode_t dev_mode = (vfs_mode_t){
			.read = dev->read != NULL,
			.write = dev->write != NULL
		};
		vfs_file_t dev_stream = vfs_Open(dev_path, dev_mode);
		free(dev_path);

		dev->function(dev->opaque, VFS_DEV_FUNC_SCAN_PARTITIONS, dev_stream);
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
	if(streamid == -1ul)
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

int vfs_syscall_mount(const char *mountpoint, const char *device)
{
	if(!vmm_userspacePointerValid(mountpoint, strlen(mountpoint)))
		return -1;
	if(!vmm_userspacePointerValid(device, strlen(device)))
		return -1;
	return vfs_Mount(mountpoint, device);
}

int vfs_syscall_unmount(const char *mountpoint)
{
	if(!vmm_userspacePointerValid(mountpoint, strlen(mountpoint)))
		return -1;
	return vfs_Unmount(mountpoint);
}
#endif
