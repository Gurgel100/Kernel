/*
 * devicemng.h
 *
 *  Created on: 16.08.2013
 *      Author: pascal
 */

#ifndef DEVICEMNG_H_
#define DEVICEMNG_H_

#include "cdi.h"
#include "list.h"
#include "stdint.h"
#include "stddef.h"

typedef struct{
	struct cdi_device *device;
	list_t partitions;
}device_t;

void dmng_Init(void);
void dmng_registerDevice(struct cdi_device *dev);
size_t dmng_Read(device_t *dev, uint64_t start, size_t size, void *buffer);

#endif /* DEVICEMNG_H_ */
