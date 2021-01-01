/*
 * partition.c
 *
 *  Created on: 06.08.2014
 *      Author: pascal
 */

#include "partition.h"
#include "storage.h"
#include "scsi.h"
#include "fs.h"
#include "lists.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "drivermanager.h"

#define MIN(val1, val2) ((val1 < val2) ? val1 : val2)

typedef enum{
	PART_TYPE_NONE = 0x00,
	PART_TYPE_LINUX = 0x83,
	PART_TYPE_ISO9660 = 0xCD,
	PART_TYPE_LAST = 0xFF
}__attribute__((packed))part_type_t;

typedef struct{
	struct{
		uint8_t Bootflag;
		uint8_t Head;
		uint8_t Sector : 6;
		uint8_t CylinderHi : 2;
		uint8_t CylinderLo;
		part_type_t Type;
		uint8_t lastHead;
		uint8_t lastSector : 6;
		uint8_t lastCylinderHi : 2;
		uint8_t lastCylinderLo;
		uint32_t firstLBA;
		uint32_t Length;
	}__attribute__((packed))entry[4];
}__attribute__((packed))PartitionTable_t;

typedef struct{
	uint8_t sector;
	uint8_t head;
	uint8_t cylinder;
}chs_t;

typedef struct{
	uint64_t id;
	char *name;
	vfs_device_t *vfs_dev;
	vfs_file_t dev_stream;
	vfs_filesystem_t *fs;
	size_t lbaStart, lbaSize, blocksize;
	part_type_t type;
}partition_t;

static const char *fs_drivers[] = {
[PART_TYPE_LINUX]	"ext2",
[PART_TYPE_ISO9660]	"iso9660",
};

static void freeFilesystem(const void *fs_p)
{
	vfs_filesystem_t *fs = (vfs_filesystem_t*)fs_p;
	fs->fs.driver->fs_destroy(&fs->fs);
	vfs_Close(fs->fs.osdep.fp);
	free(fs);
}

/*
 * Erzeugt die Dateisystemstruktur und füllt diese mit dem richtigen Treiber.
 * Parameter:	part = Partition, für die das Dateisystem gesucht werden soll
 * Rückgabe:	Zeiger auf Dateisystemstruktur oder NULL wenn kein passendes Dateisystem gefunden
 */
static vfs_filesystem_t *getFilesystem(partition_t *part)
{
	int retry_count = 0;
retry:
	if(part->fs == NULL)
	{
		vfs_filesystem_t *fs = calloc(1, sizeof(*fs));
		if(fs == NULL)
			return NULL;

		REFCOUNT_INIT(fs, freeFilesystem);

		fs->device = part->vfs_dev;
		fs->fs.driver = (struct cdi_fs_driver*)drivermanager_getDriver(fs_drivers[part->type]);

		if(fs->fs.driver == NULL)
		{
			free(fs);
			return NULL;
		}

		vfs_mode_t mode = (vfs_mode_t){
			.read = true,
			.write = true
		};
		char *path;
		asprintf(&path, "dev/%s", part->name);
		fs->fs.osdep.fp = vfs_Open(path, mode);
		free(path);

		if(fs->fs.osdep.fp == (vfs_file_t)-1)
		{
			free(fs);
			return NULL;
		}

		if(!fs->fs.driver->fs_init(&fs->fs))
		{
			vfs_Close(fs->fs.osdep.fp);
			free(fs);
			return NULL;
		}

		part->fs = fs;
	}
	else
	{
		if((part->fs = REFCOUNT_RETAIN(part->fs)) == NULL)
		{
			if(retry_count++ < 3)
				goto retry;
			else
				return NULL;
		}
	}

	return part->fs;
}

/*
 * Liest Daten von einer Partition
 */
static size_t partition_Read(void *p, uint64_t start, size_t size, void *buffer)
{
	partition_t *part = p;
	uint64_t corrected_start = MIN(start, part->lbaSize * part->blocksize);
	return vfs_Read(part->dev_stream, part->lbaStart * part->blocksize + corrected_start, MIN(part->lbaSize * part->blocksize - corrected_start, size), buffer);
}

/*
 * Schreibt Daten auf eine Partition
 */
static size_t partition_Write(void *p, uint64_t start, size_t size, const void *buffer)
{
	partition_t *part = p;
	uint64_t corrected_start = MIN(start, part->lbaSize * part->blocksize);
	return vfs_Write(part->dev_stream, part->lbaStart * part->blocksize + corrected_start, MIN(part->lbaSize * part->blocksize - corrected_start, size), buffer);
}

/*
 * Gibt bestimmte Werte zurück, welche vom VFS verwendet werden
 */
static void *partition_function(void *p, vfs_device_function_t function, ...)
{
	void *val;
	partition_t *part = p;
	vfs_filesystem_t *prev_fs = part->fs;

	switch(function)
	{
		case VFS_DEV_FUNC_TYPE:
			val = (void*)VFS_DEVICE_PARTITION;
		break;
		case VFS_DEV_FUNC_NAME:
			val = part->name;
		break;
		case VFS_DEV_FUNC_MOUNT:
			val = getFilesystem(part);
		break;
		case VFS_DEV_FUNC_UMOUNT:
			if(REFCOUNT_RELEASE(part->fs))
				__sync_bool_compare_and_swap(&part->fs, prev_fs, NULL);
			val = NULL;
		break;
		default:
			val = NULL;
	}

	return val;
}

static vfs_device_capabilities_t partition_getCapabilities(void *p __attribute__((unused)))
{
	return VFS_DEV_CAP_MOUNTABLE;
}

/*
 * Liest die Partitionstabelle aus.
 * Parameter:	dev = CDI-Gerätestruktur, das das Gerät beschreibt
 * Rückgabe:	!0 bei Fehler
 */
int partition_getPartitions(const char *dev_name, vfs_file_t dev_stream, void(*partition_callback)(void *context, void*), void *context)
{
	//Ersten Sektor auslesen
	void *buffer = malloc(512);
	if(vfs_Read(dev_stream, 0, 512, buffer) == 0)
		return 1;

	//Gültige Partitionstabelle?
	uint16_t *sig = buffer + 0x1FE;
	if(*sig != 0xAA55)
		return -1;

	//Partitionstabelle durchsuchen
	PartitionTable_t *ptable = buffer + 0x1BE;
	uint8_t i;
	for(i = 0; i < 4; i++)
	{
		if(ptable->entry[i].Type)
		{
			partition_t *part = malloc(sizeof(partition_t));
			part->id = i + 1;
			asprintf(&part->name, "%s_%hhu", dev_name, i);
			part->lbaStart = ptable->entry[i].firstLBA;
			part->lbaSize = ptable->entry[i].Length;
			part->type = ptable->entry[i].Type;
			part->dev_stream = dev_stream;
			part->blocksize = vfs_getFileinfo(dev_stream, VFS_INFO_BLOCKSIZE);
			part->fs = NULL;
			if(fs_drivers[part->type] == NULL)
			{
				free(part->name);
				free(part);
				continue;
			}

			if(partition_callback != NULL)
				partition_callback(context, part);

			if(part->type == PART_TYPE_ISO9660)
			{
				//Korigiere Start und Grösse der Partition, weil das Dateisystem selber korrigiert
				//XXX: Vielleicht gibt es einen anderen Weg es besser zu machen
				part->lbaStart = 0;
				part->lbaSize = -1ul;
			}

			//Partition beim VFS anmelden
			part->vfs_dev = malloc(sizeof(vfs_device_t));
			part->vfs_dev->read = partition_Read;
			part->vfs_dev->write = partition_Write;
			part->vfs_dev->function = partition_function;
			part->vfs_dev->getCapabilities = partition_getCapabilities;
			part->vfs_dev->opaque = part;
			vfs_RegisterDevice(part->vfs_dev);
		}
	}
	return 0;
}
