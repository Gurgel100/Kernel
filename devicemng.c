/*
 * devicemng.c
 *
 *  Created on: 16.08.2013
 *      Author: pascal
 */

#include "devicemng.h"
#include "lists.h"

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
