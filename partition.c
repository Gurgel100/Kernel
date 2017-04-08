/*
 * partition.c
 *
 *  Created on: 06.08.2014
 *      Author: pascal
 */

#ifdef BUILD_KERNEL

#include "partition.h"
#include "storage.h"
#include "scsi.h"
#include "fs.h"
#include "lists.h"
#include "vfs.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "drivermanager.h"

#define MIN(val1, val2) ((val1 < val2) ? val1 : val2)

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
		fs->fs.driver = part->fs_driver;

		vfs_mode_t mode = (vfs_mode_t){
			.read = true,
			.write = true
		};
		char *path;
		asprintf(&path, "dev/%s", part->name);
		fs->fs.osdep.fp = vfs_Open(path, mode);
		free(path);

		if(!fs->fs.driver->fs_init(&fs->fs))
			return NULL;

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
			REFCOUNT_RELEASE(part->fs);
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
			part->fs_driver = (struct cdi_fs_driver*)drivermanager_getDriver(fs_drivers[part->type]);
			if(part->fs_driver == NULL)
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

#endif
