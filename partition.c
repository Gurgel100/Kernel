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

/*
 * Erzeugt die Dateisystemstruktur und füllt diese mit dem richtigen Treiber.
 * Parameter:	part = Partition, für die das Dateisystem gesucht werden soll
 * Rückgabe:	Zeiger auf Dateisystemstruktur oder NULL wenn kein passendes Dateisystem gefunden
 */
static struct cdi_fs_filesystem *getFilesystem(partition_t *part)
{
	struct cdi_fs_filesystem *fs = calloc(1, sizeof(*fs));
	if(fs == NULL)
		return NULL;

	fs->driver = part->fs_driver;

	vfs_mode_t mode = (vfs_mode_t){
		.read = true,
		.write = true
	};
	char *path;
	asprintf(&path, "dev/%s", part->name);
	fs->osdep.fp = vfs_Open(path, mode);
	free(path);

	return fs;
}

/*
 * Liest Daten von einer Partition
 */
static size_t partition_Read(partition_t *part, uint64_t start, size_t size, void *buffer)
{
	size_t block_size = dmng_getBlockSize(part->dev);
	uint64_t corrected_start = MIN(start, part->lbaSize * block_size);
	return dmng_Read(part->dev, part->lbaStart * block_size + corrected_start, MIN(part->lbaSize * block_size - corrected_start, size), buffer);
}

/*
 * Schreibt Daten auf eine Partition
 */
static size_t partition_Write(partition_t *part, uint64_t start, size_t size, const void *buffer)
{
	return 0;
}

/*
 * Gibt bestimmte Werte zurück, welche vom VFS verwendet werden
 */
static void *partition_getValue(partition_t *part, vfs_device_function_t function)
{
	switch(function)
	{
		case FUNC_TYPE:
			return VFS_DEVICE_PARTITION;
		case FUNC_NAME:
			return part->name;
		case FUNC_DATA:
			return getFilesystem(part);
		default:
			return NULL;
	}
}

/*
 * Liest die Partitionstabelle aus.
 * Parameter:	dev = CDI-Gerätestruktur, das das Gerät beschreibt
 * Rückgabe:	!0 bei Fehler
 */
int partition_getPartitions(device_t *dev)
{
	//Ersten Sektor auslesen
	void *buffer = malloc(512);
	if(!dmng_Read(dev, 0, 512, buffer))
		return 1;

	//Gültige Partitionstabelle?
	uint16_t *sig = buffer + 0x1FE;
	if(*sig != 0xAA55)
		return -1;

	//Partitionstabelle durchsuchen
	PartitionTable_t *ptable = buffer + 0x1BE;
	uint8_t i;
	dev->partitions = list_create();
	for(i = 0; i < 4; i++)
	{
		if(ptable->entry[i].Type)
		{
			partition_t *part = malloc(sizeof(partition_t));
			part->id = i + 1;
			asprintf(&part->name, "%s_%hhu", dev->device->name, i);
			part->lbaStart = ptable->entry[i].firstLBA;
			part->lbaSize = ptable->entry[i].Length;
			part->type = ptable->entry[i].Type;
			part->dev = dev;
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
			list_push(dev->partitions, part);

			if(part->type == PART_TYPE_ISO9660)
			{
				//Korigiere Start und Grösse der Partition, weil das Dateisystem selber korrigiert
				//XXX: Vielleicht gibt es einen anderen Weg es besser zu machen
				part->lbaStart = 0;
				part->lbaSize = -1ul;
			}

			//Partition beim VFS anmelden
			vfs_device_t* vfs_dev = malloc(sizeof(vfs_device_t));
			vfs_dev->read = (vfs_device_read_handler_t*)partition_Read;
			vfs_dev->write = (vfs_device_write_handler_t*)partition_Write;
			vfs_dev->getValue = (vfs_device_getValue_handler_t*)partition_getValue;
			vfs_dev->opaque = part;
			vfs_RegisterDevice(vfs_dev);
		}
	}
	return 0;
}

#endif
