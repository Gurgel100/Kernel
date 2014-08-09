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

struct cdi_fs_filesystem *getFilesystem(partition_t *part);
struct cdi_fs_driver *getFSDriver(const char *name);

void partition_getPartitions(device_t *dev)
{
	void *buffer;
	//Art des Devices herausfinden
	if(dev->device->bus_data->bus_type == CDI_STORAGE)
	{
		struct cdi_storage_device *stdev = dev->device;
		struct cdi_storage_driver *stdrv = dev->device->driver;

		//Ersten Sektor auslesen
		buffer = malloc(stdev->block_size);
		stdrv->read_blocks(stdev, 0, 1, buffer);
	}
	else if(dev->device->bus_data->bus_type == CDI_SCSI)
	{
		struct cdi_scsi_device *scdev = dev->device;
		struct cdi_scsi_driver *scdrv = dev->device->driver;
		struct cdi_scsi_packet *packet = malloc(sizeof(*packet));

		buffer = packet->buffer = malloc(512);
		packet->bufsize = 512;
		packet->cmdsize = 12;
		packet->command[0] = 0xA8;	//Wir wollen lesen
		packet->command[2] = 0;
		packet->command[3] = 0;
		packet->command[4] = 0;
		packet->command[5] = 0;
		packet->command[9] = 1;		//1 Sektor
		packet->direction = CDI_SCSI_READ;
		scdrv->request(scdev, packet);
	}
	else
		return;
	//Partitionstabelle durchsuchen
	PartitionTable_t *ptable = buffer + 0x1BE;
	uint8_t i;
	dev->partitions = list_create();
	for(i = 0; i < 4; i++)
	{
		if(ptable->entry[i].Type)
		{
			partition_t *part = malloc(sizeof(partition_t));
			part->lbaStart = ptable->entry[i].firstLBA;
			part->size = ptable->entry[i].Length;
			part->type = ptable->entry[i].Type;
			part->dev = dev->device;
			part->fs = getFilesystem(part);
			list_push(dev->partitions, part);
		}
	}
}

struct cdi_fs_filesystem *getFilesystem(partition_t *part)
{
	struct cdi_fs_filesystem *fs = malloc(sizeof(*fs));
	struct cdi_fs_driver *driver;

	switch(part->type)
	{
		case PART_TYPE_ISO9660:
			if(!(driver = getFSDriver("iso9660")))
				goto exit_error;
			driver->fs_init(fs);
			fs->driver = driver;
		break;
		default:
			goto exit_error;
	}

	return fs;

	exit_error:
	free(fs);
	return NULL;
}

struct cdi_fs_driver *getFSDriver(const char *name)
{
	struct cdi_driver *driver;

	size_t i = 0;
	extern drivers;
	while((driver = cdi_list_get(drivers, i)))
	{
		if(driver->type == CDI_FILESYSTEM && strcmp(name, driver->name) == 0)
			return (struct cdi_fs_driver*)driver;
		i++;
	}

	return NULL;
}
