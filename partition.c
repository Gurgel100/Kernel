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

#define MIN(val1, val2) ((val1 < val2) ? val1 : val2)

struct cdi_fs_filesystem *getFilesystem(partition_t *part);
struct cdi_fs_driver *getFSDriver(const char *name);

/*
 * Liest Daten von einer Partition
 */
static size_t partition_Read(partition_t *part, uint64_t start, size_t size, const void *buffer)
{
	uint64_t corrected_start = MIN(start, part->size);
	return dmng_Read(part->dev, part->lbaStart + corrected_start, MIN(part->size - corrected_start, size), buffer);
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
			return part->fs;
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
			part->size = ptable->entry[i].Length;
			part->type = ptable->entry[i].Type;
			part->dev = dev;
			part->fs = getFilesystem(part);
			list_push(dev->partitions, part);

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

/*
 * Erzeugt die Dateisystemstruktur und füllt diese mit dem richtigen Treiber.
 * Parameter:	part = Partition, für die das Dateisystem gesucht werden soll
 * Rückgabe:	Zeiger auf Dateisystemstruktur oder NULL wenn kein passendes Dateisystem gefunden
 */
struct cdi_fs_filesystem *getFilesystem(partition_t *part)
{
	struct cdi_fs_filesystem *fs = malloc(sizeof(*fs));
	struct cdi_fs_driver *driver;

	switch(part->type)
	{
		case PART_TYPE_ISO9660:
			if(!(driver = getFSDriver("iso9660")))
				goto exit_error;
			fs->driver = driver;
			vfs_mode_t mode = {
					.read = true,
					.write = true
			};
			char *path;
			asprintf(&path, "dev/%s", part->dev->device->name);
			fs->osdep.fp = vfs_Open(NULL, path, mode);
			free(path);
			//asprintf(&fs->osdep.devPath, "dev/%s", part->dev->name);
		break;
		default:
			goto exit_error;
	}

	return fs;

	exit_error:
	free(fs);
	return NULL;
}

/*
 * Sucht den Dateisystemtreiber mit dem angebenen Namen
 * Parameter:	name = Name des Dateisystemtreibers, nach dem gesucht werden soll
 * Rückgabe:	Zeiger auf Dateisystemtreiberstruktur oder NULL falls kein passender Treiber gefunden wurde
 */
struct cdi_fs_driver *getFSDriver(const char *name)
{
	struct cdi_driver *driver;

	size_t i = 0;
	extern cdi_list_t drivers;
	while((driver = cdi_list_get(drivers, i)))
	{
		if(driver->type == CDI_FILESYSTEM && strcmp(name, driver->name) == 0)
			return (struct cdi_fs_driver*)driver;
		i++;
	}

	return NULL;
}

#endif
