/*
 * devicemng.c
 *
 *  Created on: 16.08.2013
 *      Author: pascal
 */

#include "devicemng.h"
#include "lists.h"
#include "storage.h"
#include "scsi.h"

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
		struct cdi_storage_driver *driver = dev->device->driver;
		struct cdi_storage_device *device = dev->device;
		uint64_t block_start = start / device->block_size;
		uint64_t block_count = size / device->block_size + ((size % device->block_size) ? 1 : 0);
		if(block_start + block_count > device->block_count)
			block_count = device->block_count - block_start;
		void *block_buffer = malloc(device->block_size * block_count);

		if(driver->read_blocks(device, block_start, block_count, block_buffer))
			return 0;

		memcpy(buffer, block_buffer + block_start % device->block_size, size);
		free(block_buffer);
	}
	else if(dev->device->bus_data->bus_type == CDI_SCSI)
	{
		struct cdi_scsi_driver *driver = dev->device->driver;
		struct cdi_scsi_device *device = dev->device;

		uint32_t lba = start / 512;
		uint64_t tmp_size = MIN(size, 512);
		uint64_t offset = 0;
		void *tmp_buffer = malloc(512);
		do
		{
			uint32_t length = 1;
			struct cdi_scsi_packet packet = {
				.buffer = tmp_buffer,
				.bufsize = 512,
				.cmdsize = 12,
				.command = {0xA8, 0, GET_BYTE(lba, 0x18), GET_BYTE(lba, 0x10), GET_BYTE(lba, 0x08), GET_BYTE(lba, 0x00),
						GET_BYTE(length, 0x18), GET_BYTE(length, 0x10), GET_BYTE(length, 0x08), GET_BYTE(length, 0x00), 0, 0},
				.direction = CDI_SCSI_READ
			};

			if(driver->request(device, &packet))
				return 0;

			memcpy(buffer + offset, tmp_buffer, tmp_size);
			offset += tmp_size;
			tmp_size = MIN(size - offset, 512);
		}
		while(offset < size);
		free(tmp_buffer);
	}
	else
		return 0;

	return size;
}
