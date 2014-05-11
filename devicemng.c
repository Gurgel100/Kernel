/*
 * devicemng.c
 *
 *  Created on: 16.08.2013
 *      Author: pascal
 */

#include "devicemng.h"
#include "lists.h"

extern drivers;

void dmng_Init()
{
	struct cdi_driver *driver;
	uint64_t i;
	//Durchsuche Treiberliste
	for(i = 0; i < cdi_list_size(drivers); i++)
	{
		driver = cdi_list_get(drivers, i);
		if(!driver)
			break;
		if(driver->type == CDI_STORAGE)
		{
		}
	}
}
