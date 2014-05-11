/*
 * devicemng.h
 *
 *  Created on: 16.08.2013
 *      Author: pascal
 */

#ifndef DEVICEMNG_H_
#define DEVICEMNG_H_

#include "cdi.h"

typedef struct{
		struct cdi_device_t *device;
}partition_t;

void dmng_Init(void);

#endif /* DEVICEMNG_H_ */
