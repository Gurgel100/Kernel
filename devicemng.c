/*
 * devicemng.c
 *
 *  Created on: 16.08.2013
 *      Author: pascal
 */

#include "devicemng.h"
#include "partition.h"
#include "lists.h"
#include "storage.h"
#include "scsi.h"
#include "stdlib.h"
#include "string.h"
#include "assert.h"
#include "stdarg.h"

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
	vfs_dev->read = dmng_Read;
	vfs_dev->write = dmng_Write;
	vfs_dev->function = dmng_function;
	vfs_dev->getCapabilities = dmng_getCapabilities;

	vfs_RegisterDevice(vfs_dev);

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
size_t dmng_Read(void *d, uint64_t start, size_t size, void *buffer)
{
	device_t *dev = d;
	if(size == 0 || buffer == NULL) return 0;

	if(dev->device->bus_data->bus_type == CDI_STORAGE)
	{
		struct cdi_storage_driver *driver = (struct cdi_storage_driver*)dev->device->driver;
		struct cdi_storage_device *device = (struct cdi_storage_device*)dev->device;
		uint64_t block_start = start / device->block_size;
		uint64_t start_offset = start % device->block_size;
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

		memcpy(buffer, block_buffer + start_offset, size);
		free(block_buffer);
	}
	else if(dev->device->bus_data->bus_type == CDI_SCSI)
	{
		struct cdi_scsi_driver *driver = (struct cdi_scsi_driver*)dev->device->driver;
		struct cdi_scsi_device *device = (struct cdi_scsi_device*)dev->device;

		uint32_t lba = start / 2048;
		uint32_t start_offset = start % 2048;
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

			memcpy(buffer + offset, tmp_buffer + start_offset, tmp_size);
			start_offset = 0;
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

size_t dmng_Write(void *d, uint64_t start, size_t size, const void *buffer)
{
	device_t *dev = d;
	if(size == 0 || buffer == NULL) return 0;

	if(dev->device->bus_data->bus_type == CDI_STORAGE)
	{
		struct cdi_storage_driver *driver = (struct cdi_storage_driver*)dev->device->driver;
		struct cdi_storage_device *device = (struct cdi_storage_device*)dev->device;
		uint64_t block_start = start / device->block_size;
		uint64_t start_offset = start % device->block_size;
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
		memcpy(block_buffer + start_offset, buffer, size);
		if(driver->write_blocks(device, block_start, block_count, block_buffer))
		{
			semaphore_release(&dev->semaphore);
			return 0;
		}
		semaphore_release(&dev->semaphore);

		free(block_buffer);
	}
	else if(dev->device->bus_data->bus_type == CDI_SCSI)
	{
		struct cdi_scsi_driver *driver = (struct cdi_scsi_driver*)dev->device->driver;
		struct cdi_scsi_device *device = (struct cdi_scsi_device*)dev->device;

		uint32_t lba = start / 2048;
		uint32_t start_offset = start % 2048;
		uint64_t tmp_size = MIN(size, 2048);
		uint64_t offset = 0;
		void *tmp_buffer = malloc(2048);
		do
		{
			uint32_t length = 1;
			struct cdi_scsi_packet read_packet = {
				.buffer = tmp_buffer,
				.bufsize = 2048,
				.cmdsize = 12,
				.command = {0xA8, 0, GET_BYTE(lba, 0x18), GET_BYTE(lba, 0x10), GET_BYTE(lba, 0x08), GET_BYTE(lba, 0x00),
						GET_BYTE(length, 0x18), GET_BYTE(length, 0x10), GET_BYTE(length, 0x08), GET_BYTE(length, 0x00), 0, 0},
				.direction = CDI_SCSI_READ
			};
			struct cdi_scsi_packet write_packet = {
				.buffer = tmp_buffer,
				.bufsize = 2048,
				.cmdsize = 12,
				.command = {0xAA, 0, GET_BYTE(lba, 0x18), GET_BYTE(lba, 0x10), GET_BYTE(lba, 0x08), GET_BYTE(lba, 0x00),
						GET_BYTE(length, 0x18), GET_BYTE(length, 0x10), GET_BYTE(length, 0x08), GET_BYTE(length, 0x00), 0, 0},
				.direction = CDI_SCSI_WRITE
			};

			//Gerät reservieren
			semaphore_acquire(&dev->semaphore);
			if(driver->request(device, &read_packet))
			{
				semaphore_release(&dev->semaphore);
				return 0;
			}
			memcpy(tmp_buffer + start_offset, buffer + offset, tmp_size);
			if(driver->request(device, &write_packet))
			{
				semaphore_release(&dev->semaphore);
				return 0;
			}
			semaphore_release(&dev->semaphore);

			start_offset = 0;
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

static void add_partition(void *d, void *p)
{
	device_t *dev = d;
	list_push(dev->partitions, p);
}

void *dmng_function(void *d, vfs_device_function_t function, ...)
{
	void *val;
	device_t *dev = d;

	va_list arg;
	va_start(arg, function);

	switch(function)
	{
		case VFS_DEV_FUNC_TYPE:
			val = (void*)VFS_DEVICE_STORAGE;
		break;
		case VFS_DEV_FUNC_NAME:
			val = (void*)dev->device->name;
		break;
		case VFS_DEV_FUNC_SCAN_PARTITIONS:
			dev->partitions = list_create();
			partition_getPartitions(dev->device->name, va_arg(arg, vfs_file_t), add_partition, dev);
			val = 0;
		break;
		case VFS_DEV_FUNC_BLOCKSIZE:
			val = (void*)dmng_getBlockSize(dev);
		break;
		default:
			val = NULL;
	}

	va_end(arg);
	return val;
}

vfs_device_capabilities_t dmng_getCapabilities(void *d __attribute__((unused)))
{
	return VFS_DEV_CAP_BLOCKSIZE | VFS_DEV_CAP_PARTITIONS;
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

