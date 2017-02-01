/*
 * scsi.c
 *
 *  Created on: 02.08.2013
 *      Author: pascal
 */

#include "scsi.h"
#include "stdlib.h"
#include "devicemng.h"

/**
 * Initialisiert die Datenstrukturen fuer einen SCSI-Treiber
 */
void cdi_scsi_driver_init(struct cdi_scsi_driver* driver)
{
	driver->drv.type = CDI_SCSI;
	cdi_driver_init((struct cdi_driver*)driver);
}

/**
 * Deinitialisiert die Datenstrukturen fuer einen SCSI-Treiber
 */
void cdi_scsi_driver_destroy(struct cdi_scsi_driver* driver)
{
	cdi_driver_destroy((struct cdi_driver*)driver);
}

/**
 * Initialisiert ein neues SCSI-Geraet
 *
 * Der Typ der Geraetes muss bereits gesetzt sein
 */
void cdi_scsi_device_init(struct cdi_scsi_device* device)
{
	device->dev.bus_data = malloc(sizeof(struct cdi_bus_data));
	device->dev.bus_data->bus_type = CDI_SCSI;
	dmng_registerDevice((struct cdi_device*)device);
}
