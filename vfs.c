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
#include "partition.h"

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

typedef enum{
	TYPE_DIR, TYPE_FILE, TYPE_MOUNT, TYPE_LINK, TYPE_DEV
}vfs_node_type_t;

typedef struct vfs_node{
		char *Name;
		vfs_node_type_t Type;
		struct vfs_node *Parent;
		struct vfs_node *Child;	//Ungültig wenn kein TYPE_DIR oder TYPE_MOUNT. Bei TYPE_LINK -> Link zum Verknüpften Element
		struct vfs_node *Next;
		union{
			partition_t *partition;
			device_t *dev;
		};
}vfs_node_t;

size_t getDirs(char ***Dirs, const char *Path);
vfs_node_t *getNode(const char *Path);
char *splitPath(const char *path, char **file, char **dev);

static vfs_node_t root;
static vfs_node_t *lastNode;

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

	lastNode = Node;
}

/*
 * In eine Datei schreiben
 * Parameter:	Path = Pfad zur Datei als String
 */
void vfs_Read(const char *Path, const void *Buffer)
{
	bool root = false;
	size_t i, length;
	char **Dirs = NULL;

	if(Buffer == NULL || Path == NULL)
		return;

	if(Path[0] == VFS_SEPARATOR)
		root = true;

	length = getDirs(&Dirs, Path);

	//Ausgeben
	if(root)
		printf("VFS: 0 = /\n");
	for(i = 0; i < length; i++)
	{
		if(Dirs[i] == NULL)
			break;
		printf("VFS: %u = %s\n", i + 1, Dirs[i]);
	}
	free(Dirs);
}

void vfs_Write(const char *Path, const void *Buffer)
{
	bool root = false;
	size_t i, length;
	char **Dirs = NULL;

	if(Buffer == NULL || Path == NULL)
		return;

	if(Path[0] == VFS_SEPARATOR)
		root = true;

	length = getDirs(&Dirs, Path);

	//Ausgeben
	if(root)
		printf("VFS: 0 = /\n");
	for(i = 0; i < length; i++)
	{
		if(Dirs[i] == NULL)
			break;
		printf("VFS: %u = %s\n", i + 1, Dirs[i]);
	}
	free(Dirs);
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
		vfs_node_t *new = malloc(sizeof(vfs_node_t));
		asprintf(&new->Name, "%s%u", device->device->name, part->id);
		new->Type = TYPE_MOUNT;
		new->Parent = mount;
		new->Next = mount->Child;
		mount->Child = new;
		new->partition = part;
		//Dateisystem initialisieren
		((struct cdi_fs_driver*)part->dev->driver)->fs_init(part->fs);
	}
	if(!i)
		return 1;

	return 0;
}

/*
 * Unmountet ein Dateisystem am entsprechenden Mountpoint (Mount)
 * Parameter:	Mount = Mountpoint (Pfad)
 *///TODO
bool vfs_Unmount(const char *Mount)
{
	//Liste aller Mountpoints durchsuchen
	/*vfs_mount_t *MountPoint;
	for(MountPoint = &root; MountPoint; MountPoint = MountPoint->Next)
	{
		if(MountPoint == Mount)	//Mount gefunden
			return true;
	}*/
	return false;
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

	if(!*Dirs)
		*Dirs = malloc(sizeof(char*));

	(*Dirs)[0] = strtok(Path, VFS_ROOT);
	for(i = 1; ; i++)
	{
		if((tmp = strtok(NULL, VFS_ROOT)) ==
		 NULL)
			break;
		*Dirs = realloc(*Dirs, sizeof(char*) * (i + 1));
		(*Dirs)[i] = tmp;
	}
	return i;
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

	free(Dirs);
	return Node;
}

/*
 * Registriert ein Gerät. Dazu wird eine Gerätedatei im Verzeichniss /dev angelegt.
 * Parameter:	dev = Gerätestruktur
 */
void vfs_RegisterDevice(device_t *dev)
{
	static bool first = false;
	const char *Path = "/dev";	//Pfad zu den Gerätendateien
	vfs_node_t *Node, *tmp;
	//ist der Ordner schon vorhanden?
	if(!(tmp = getNode(Path))) return;	//Fehler

	//Gerätedatei anlegen
	//wird vorne angelegt
	Node = malloc(sizeof(*Node));
	Node->Type = TYPE_DEV;
	Node->dev = dev;
	Node->Next = tmp->Child;
	tmp->Child = Node;
	Node->Name = dev->device->name;
	Node->Parent = tmp;

	//Wenn es das erste Gerät ist, dann mounten
	if(!first)
	{
		first = true;
		char *DevPath;
		asprintf(&DevPath, "%s/%s", Path, Node->Name);
		vfs_Mount("/mount", DevPath);
		free(DevPath);
	}
}
