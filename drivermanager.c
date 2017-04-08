/*
 * drivermanager.c
 *
 *  Created on: 05.04.2017
 *      Author: pascal
 */

#include "drivermanager.h"
#include "list.h"
#include "assert.h"
#include "stddef.h"
#include "string.h"
#include "stdbool.h"

static list_t drivers;

void drivermanager_init()
{
	drivers = list_create();
	assert(drivers != NULL);
}

void drivermanager_registerDriver(struct cdi_driver *drv)
{
	assert(drv != NULL);
	list_push(drivers, drv);
}

struct cdi_driver *drivermanager_getDriver(const char *name)
{
	struct cdi_driver *drv;
	size_t i = 0;

	assert(name != NULL);

	while((drv = list_get(drivers, i++)) != NULL)
	{
		if(strcmp(name, drv->name) == 0)
			return drv;
	}

	return drv;
}

struct cdi_driver *drivermanager_getNextDeviceDriver(cdi_device_type_t bus, struct cdi_driver *prev)
{
	struct cdi_driver *drv;
	size_t i = 0;
	bool prev_found = prev == NULL;

	while((drv = list_get(drivers, i++)) != NULL)
	{
		if(prev_found && drv->bus == bus)
			return drv;
		if(drv == prev)
			prev_found = true;
	}

	return drv;
}
