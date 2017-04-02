/*
 * devicemng.c
 *
 *  Created on: 16.08.2013
 *      Author: pascal
 */

#ifdef BUILD_KERNEL

#include "devicemng.h"
#include "partition.h"
#include "lists.h"
#include "storage.h"
#include "scsi.h"
#include "stdlib.h"
#include "string.h"
#include "assert.h"

#define GET_BYTE(value, offset) (value >> offset) & 0xFF
#define MIN(val1, val2) ((val1 < val2) ? val1 : val2)

static list_t devices;

void dmng_Init()
{
	//Initialisiere Geräteliste
	devices = list_create();
}

void dmng_registerDevice(struct cdi_device *dev)
{
	device_t *device = malloc(sizeof(device_t));
	device->partitions = list_create();
	device->device = dev;
	semaphore_init(&device->semaphore, 1);

	vfs_device_t *vfs_dev = malloc(sizeof(vfs_device_t));
	vfs_dev->opaque = device;
	vfs_dev->read = (vfs_device_read_handler_t*)dmng_Read;
	vfs_dev->write = (vfs_device_write_handler_t*)NULL;
	vfs_dev->getValue = (vfs_device_getValue_handler_t*)dmng_getValue;

	vfs_RegisterDevice(vfs_dev);

	partition_getPartitions(device);

	list_push(devices, device);
}

/*
 * Liest von einem Datenträger
 * Parameter:	dev = Gerät von dem gelesen werden soll
 * 				start = Byte an dem angefangen werden soll zu lesen
 * 				size = Anzahl Bytes die gelesen werden sollen
 * 				buffer = Buffer in den die Daten geschrieben werden sollen
 * Rückgabe:	0 bei Fehler und sonst die gelesenen Bytes
 */
size_t dmng_Read(device_t *dev, uint64_t start, size_t size, void *buffer)
{
	if(size == 0 || buffer == NULL) return 0;

	if(dev->device->bus_data->bus_type == CDI_STORAGE)
	{
		struct cdi_storage_driver *driver = (struct cdi_storage_driver*)dev->device->driver;
		struct cdi_storage_device *device = (struct cdi_storage_device*)dev->device;
		uint64_t block_start = start / device->block_size;
		uint64_t block_count = size / device->block_size + ((size % device->block_size) ? 1 : 0);
		if(block_start + block_count > device->block_count)
			block_count = device->block_count - block_start;
		void *block_buffer = malloc(device->block_size * block_count);

		//Gerät reservieren
		semaphore_acquire(&dev->semaphore);
		if(driver->read_blocks(device, block_start, block_count, block_buffer))
		{
			semaphore_release(&dev->semaphore);
			return 0;
		}
		semaphore_release(&dev->semaphore);

		memcpy((void*)buffer, block_buffer + block_start % device->block_size, size);
		free(block_buffer);
	}
	else if(dev->device->bus_data->bus_type == CDI_SCSI)
	{
		struct cdi_scsi_driver *driver = (struct cdi_scsi_driver*)dev->device->driver;
		struct cdi_scsi_device *device = (struct cdi_scsi_device*)dev->device;

		uint32_t lba = start / 2048;
		uint64_t tmp_size = MIN(size, 2048);
		uint64_t offset = 0;
		void *tmp_buffer = malloc(2048);
		do
		{
			uint32_t length = 1;
			struct cdi_scsi_packet packet = {
				.buffer = tmp_buffer,
				.bufsize = 2048,
				.cmdsize = 12,
				.command = {0xA8, 0, GET_BYTE(lba, 0x18), GET_BYTE(lba, 0x10), GET_BYTE(lba, 0x08), GET_BYTE(lba, 0x00),
						GET_BYTE(length, 0x18), GET_BYTE(length, 0x10), GET_BYTE(length, 0x08), GET_BYTE(length, 0x00), 0, 0},
				.direction = CDI_SCSI_READ
			};

			//Gerät reservieren
			semaphore_acquire(&dev->semaphore);
			if(driver->request(device, &packet))
			{
				semaphore_release(&dev->semaphore);
				return 0;
			}
			semaphore_release(&dev->semaphore);

			memcpy((void*)buffer + offset, tmp_buffer + start % 2048 + offset, tmp_size);
			offset += tmp_size;
			tmp_size = MIN(size - offset, 2048);
		}
		while(offset < size);
		free(tmp_buffer);
	}
	else
		return 0;

	return size;
}

void *dmng_getValue(device_t *dev, vfs_device_function_t function)
{
	switch(function)
	{
		case FUNC_TYPE:
			return VFS_DEVICE_STORAGE;
		case FUNC_NAME:
			return (void*)dev->device->name;
		default:
			return NULL;
	}
}

size_t dmng_getBlockSize(device_t *dev)
{
	assert(dev != NULL);
	if(dev->device->bus_data->bus_type == CDI_STORAGE)
	{
		struct cdi_storage_device *device = (struct cdi_storage_device*)dev->device;
		return device->block_size;
	}
	else if(dev->device->bus_data->bus_type == CDI_SCSI)
		return 2048;
	else
		return 0;
}

#endif
