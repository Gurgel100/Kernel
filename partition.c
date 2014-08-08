/*
 * partition.c
 *
 *  Created on: 06.08.2014
 *      Author: pascal
 */

#include "partition.h"
#include "storage.h"
#include "scsi.h"

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

		buffer = packet->buffer = malloc(4096);
		packet->bufsize = 4096;
		packet->cmdsize = 4;
		packet->command[0]= 'R';
		packet->command[1]= 'E';
		packet->command[2]= 'A';
		packet->command[3]= 'D';
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
		if(ptable->entry[i].firstLBA)
		{
			partition_t *part = malloc(sizeof(partition_t));
			part->lbaStart = ptable->entry[i].firstLBA;
			part->size = ptable->entry[i].Length;
			part->dev = dev->device;
			part->fs = getPartitionFS(part);
			list_push(dev->partitions, part);
		}
	}
}

struct cdi_fs_driver *getPartitionFS(partition_t *part)
{
	struct cdi_fs_driver *fs = malloc(sizeof(*fs));

	return fs;
}
