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

size_t getDirs(char ***Dirs, const char *Path);
vfs_node_t *getLastNode(const char *Path, char **remPath);
vfs_node_t *getNode(const char *Path);
char *splitPath(const char *path, char **file, char **dev);
struct cdi_fs_res *getRes(struct cdi_fs_stream *stream, const char *path);

static vfs_node_t root;
static vfs_node_t *lastNode;
static uint8_t nextPartID = 0;

void vfs_Init(void)
{
	//Root
	root.Name = VFS_ROOT;
	root.Next = NULL;
	root.Child = NULL;
	root.Parent = &root;
	root.Type = TYPE_DIR;

	//Virtuelle Ordner anlegen
	vfs_node_t *Node;
	//Unterordner "dev" anlegen: für Gerätedateien
	Node = malloc(sizeof(vfs_node_t));
	Node->Next = NULL;
	Node->Parent = &root;
	Node->Child = NULL;
	Node->Name = "dev";
	Node->Type = TYPE_DIR;	//Mount->fs wird nicht benötigt
	root.Child = Node;

	//Unterordner "sysinf" anlegen: für Systeminformationen
	Node->Next = malloc(sizeof(vfs_node_t));
	Node = Node->Next;
	Node->Child = NULL;
	Node->Next = NULL;
	Node->Parent = &root;
	Node->Name = "sysinf";
	Node->Type = TYPE_DIR;

	//Unterordner "mount" anlegen: für Mountpoints
	Node->Next = malloc(sizeof(vfs_node_t));
	Node = Node->Next;
	Node->Child = NULL;
	Node->Next = NULL;
	Node->Parent = &root;
	Node->Name = "mount";
	Node->Type = TYPE_DIR;

	//Unterordner "dev" füllen
	Node = getNode("/dev");
	if(Node != NULL)
	{
		Node->Child = malloc(sizeof(vfs_node_t));
		Node->Child->Parent = Node;
		Node = Node->Child;
		Node->Name = "stdout";
		Node->Type = TYPE_FILE;

		Node->Next = malloc(sizeof(vfs_node_t));
		Node->Next->Parent = Node->Parent;
		Node = Node->Next;
		Node->Name = "stdin";
		Node->Type = TYPE_FILE;

		Node->Next = malloc(sizeof(vfs_node_t));
		Node->Next->Parent = Node->Parent;
		Node = Node->Next;
		Node->Name = "stderr";
		Node->Type = TYPE_FILE;
	}

	lastNode = Node;
}

/*
 * Eine Datei öffnen
 * Parameter:	path = Pfad zur Datei
 * 				mode = Modus, in der die Datei geöffnet werden soll
 */
vfs_stream_t *vfs_Open(const char *path, vfs_mode_t mode)
{
	if(path == NULL || strlen(path) == 0 || (!mode.read && !mode.write))
		return NULL;

	char *remPath;
	vfs_stream_t *stream = calloc(1, sizeof(*stream));
	stream->mode = mode;

	vfs_node_t *node = getLastNode(path, &remPath);
	stream->node = node;
	switch(node->Type)
	{
		case TYPE_MOUNT:
			stream->stream.fs = node->partition->fs;
			stream->stream.res = getRes(&stream->stream, remPath);
			if(stream->stream.res == NULL)
			{
				free(stream);
				if(remPath)
					free(remPath);
				return NULL;
			}
		break;
		case TYPE_DIR:	//Ordner kann man nicht öffnen
			printf("Versucht Ordner zu öffnen!\n");
			free(stream);
			if(remPath)
				free(remPath);
			return NULL;
		break;
	}

	if(mode.empty)
	{
		if(stream->stream.fs->read_only && !stream->stream.res->flags.write)
		{
			stream->stream.res->res->unload(&stream->stream);
			free(stream);
			return NULL;
		}
		stream->stream.res->file->truncate(&stream->stream, 0);
	}
	if(remPath)
		free(remPath);

	return stream;
}

/*
 * Eine Datei schliessen
 * Parameter:	stream = Stream der geschlossen werden soll
 */
void vfs_Close(vfs_stream_t *stream)
{
	if(stream != NULL)
		return;

	switch(stream->node->Type)
	{
		case TYPE_MOUNT:
			stream->stream.res->res->unload(&stream->stream);
		break;
	}
	free(stream);
}

/*
 * Eine Datei lesen
 * Parameter:	Path = Pfad zur Datei als String
 * 				start = Anfangsbyte, an dem angefangen werden soll zu lesen
 * 				length = Anzahl der Bytes, die gelesen werden sollen
 * 				Buffer = Buffer in den die Bytes geschrieben werden
 */
size_t vfs_Read(vfs_stream_t *stream, uint64_t start, size_t length, const void *buffer)
{
	if(stream == NULL || buffer == NULL)
		return 0;

	size_t sizeRead = 0;

	//Erst überprüfen ob der Stream zum lesen geöffnet wurde
	if(!stream->mode.read)
		return 0;

	switch(stream->node->Type)
	{
		case TYPE_DEV:
			sizeRead = dmng_Read(stream->node->dev, start, length, buffer);
		break;
		case TYPE_MOUNT:
			sizeRead = stream->stream.res->file->read(&stream->stream, start, length, buffer);
		break;
	}
	return sizeRead;
}

size_t vfs_Write(vfs_stream_t *stream, uint64_t start, size_t length, const void *buffer)
{
	if(stream == NULL || buffer == NULL)
		return 0;

	size_t sizeWritten = 0;

	//Erst überprüfen ob der Stream zum schreiben geöffnet wurde
	if(!stream->mode.write)
		return 0;

	switch(stream->node->Type)
	{
		case TYPE_DEV:
			sizeWritten = dmng_Read(stream->node->dev, start, length, buffer);
		break;
		case TYPE_MOUNT:
			//Überprüfen, ob auf das Dateisystem geschrieben werden darf
			if(!stream->stream.fs->read_only && stream->stream.res->flags.write)
				sizeWritten = stream->stream.res->file->read(&stream->stream, start, length, buffer);
		break;
	}
	return sizeWritten;
}

/*
 * Gibt die Metainformationen einer Datei zurück
 * Parameter:	stream = stream dessen Grösse abgefragt wird (muss eine Datei sein)
 * Rückgabe:	Grösse des Streams oder 0 bei Fehler
 */
uint64_t vfs_getFileinfo(vfs_stream_t *stream, vfs_fileinfo_t info)
{
	if(stream->node->Type == TYPE_MOUNT && stream->stream.res->file != NULL)
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
		return 1;

	device_t *device = devNode->dev;

	//Jede Partition durchgehen
	partition_t *part;
	size_t i = 0;
	while((part = list_get(device->partitions, i++)))
	{
		vfs_node_t *new = calloc(1, sizeof(vfs_node_t));
		asprintf(&new->Name, "%u", nextPartID++);
		new->Type = TYPE_MOUNT;
		new->Parent = mount;
		new->Next = mount->Child;
		mount->Child = new;
		new->partition = part;
		//Dateisystem initialisieren
		if(!part->fs->driver->fs_init(part->fs))
			return 1;
	}
	if(!i)
		return 1;

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
	if(!mount || mount->Type != TYPE_MOUNT)
		return 1;

	vfs_node_t *parentNode;
	parentNode = mount->Parent;
	if(parentNode->Child == mount)
		parentNode->Child = mount->Next;
	else
		parentNode->Child->Next = mount->Next;

	//FS deinitialisieren
	mount->partition->fs->driver->fs_destroy(mount->partition->fs);

	free(mount->Name);
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
	int status;
	vfs_node_t *node = getNode("dev");
	asprintf(&DevPath, "dev/%s", node->Child->Name);
	status = vfs_Mount("/mount", DevPath);
	free(DevPath);
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

size_t getDirs(char ***Dirs, const char *Path)
{
	size_t i;
	char *tmp;
	//Erst Pfad sichern
	char *tmpPath = strdup(Path);

	if(!*Dirs)
		*Dirs = malloc(sizeof(char*));

	(*Dirs)[0] = strdup(strtok(tmpPath, VFS_ROOT));
	for(i = 1; ; i++)
	{
		if((tmp = strtok(NULL, VFS_ROOT)) ==
		 NULL)
			break;
		*Dirs = realloc(*Dirs, sizeof(char*) * (i + 1));
		(*Dirs)[i] = strdup(tmp);
	}
	free(tmpPath);
	return i;
}

void freeDirs(char ***Dirs, size_t size)
{
	size_t i;
	for(i = 0; i < size; i++)
		free((*Dirs)[i]);
	free(*Dirs);
}

/*
 * Finde die letzte Node, die sich im Pfad befindet. Der Pfad muss absolut abgeben werden.
 * Parameter:	Path = Absoluter Pfad
 * 				remPath = restlicher Pfad
 */
vfs_node_t *getLastNode(const char *Path, char **remPath)
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
		Node = Node->Child;
		while(Node && strcmp(Node->Name, Dirs[i]))
			Node = Node->Next;
		if(!Node) break;
	}
	if(!Node)
	{
		Node = oldNode;
		i--;
	}
	if(remPath != NULL)
	{
		*remPath = NULL;
		for(;i < NumDirs; i++)
		{
			*remPath = realloc(*remPath, strlen(Dirs[i]));
			strcpy(*remPath, Dirs[i]);
		}
	}

	freeDirs(&Dirs, NumDirs);
	return Node;
}

/*de
 * Finde die Node auf die der Pfad zeigt. Der Pfad muss absolut abgeben werden.
 * Parameter:	Path = Absoluter Pfad
 */
vfs_node_t *getNode(const char *Path)
{
	vfs_node_t *Node = &root;
	char **Dirs = NULL;
	size_t NumDirs, i;

	//Welche Ordner?
	NumDirs = getDirs(&Dirs, Path);
	for(i = 0; i < NumDirs; i++)
	{
		Node = Node->Child;
		while(Node && strcmp(Node->Name, Dirs[i]))
			Node = Node->Next;
		if(!Node) return NULL;
	}

	freeDirs(&Dirs, NumDirs);
	return Node;
}

/*
 * Registriert ein Gerät. Dazu wird eine Gerätedatei im Verzeichniss /dev angelegt.
 * Parameter:	dev = Gerätestruktur
 */
void vfs_RegisterDevice(device_t *dev)
{
	const char *Path = "/dev";	//Pfad zu den Gerätendateien
	vfs_node_t *Node, *tmp;
	//ist der Ordner schon vorhanden?
	if(!(tmp = getNode(Path))) return;	//Fehler

	//Gerätedatei anlegen
	//wird vorne angelegt
	Node = calloc(1, sizeof(*Node));
	Node->Type = TYPE_DEV;
	Node->dev = dev;
	Node->Next = tmp->Child;
	tmp->Child = Node;
	Node->Name = dev->device->name;
	Node->Parent = tmp;
	printf("Eingehaengt in: /dev/%s\n", Node->Name);
}

struct cdi_fs_res *getRes(struct cdi_fs_stream *stream, const char *path)
{
	struct cdi_fs_res *res;
	struct cdi_fs_res *prevRes = stream->fs->root_res;
	struct cdi_fs_stream tmpStream = {
		.fs = stream->fs,
		.res = prevRes
	};

	char **dirs = NULL;
	size_t dirSize = getDirs(&dirs, path);

	if(!prevRes->loaded)
		prevRes->res->load(&tmpStream);

	size_t i = 0;
	size_t j = 0;
	while(j < dirSize && (res = cdi_list_get(prevRes->children, i++)))
	{
		if(!strcmp(res->name, dirs[j]))
		{
			tmpStream.res = res;
			res->res->load(&tmpStream);
			prevRes = res;
			j++;
		}
	}
	freeDirs(&dirs, dirSize);

	return res;
}

#endif
