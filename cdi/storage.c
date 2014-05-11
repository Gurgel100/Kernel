/*
 * storage.c
 *
 *  Created on: 02.08.2013
 *      Author: pascal
 */

#include "storage.h"
#include "vfs.h"

/**
 * \german
 * Initialisiert die Datenstrukturen fuer einen Massenspeichertreiber
 * (erzeugt die devices-Liste). Diese Funktion muss während der
 * Initialisierung eines Massenspeichertreibers aufgerufen werden.
 * \endgerman
 * \english
 * Initialises the data structures for a mass storage device. This function
 * must be called during initialisation of a mass storage driver.
 * \endenglish
 *
 */
void cdi_storage_driver_init(struct cdi_storage_driver* driver)
{
	driver->drv.type = CDI_STORAGE;
	cdi_driver_init((struct cdi_driver*)driver);
}

/**
 * \german
 * Deinitialisiert die Datenstrukturen fuer einen Massenspeichertreiber
 * (gibt die devices-Liste frei)
 * \endgerman
 * \english
 * Deinitialises the data structures for a mass storage driver
 * \endenglish
 */
void cdi_storage_driver_destroy(struct cdi_storage_driver* driver)
{
	cdi_driver_destroy((struct cdi_driver*)driver);
}

/** TODO
 * \german
 * Meldet ein Massenspeichergerät beim Betriebssystem an. Diese Funktion muss
 * aufgerufen werden, nachdem ein Massenspeichertreiber alle notwendigen
 * Initialisierungen durchgeführt hat.
 *
 * Das Betriebssystem kann dann beispielsweise nach Partitionen auf dem Gerät
 * suchen und Gerätedateien verfügbar machen.
 * \endgerman
 * \english
 * Registers a mass storage device with the operating system. This function
 * must be called once a mass storage driver has completed all required
 * initialisations.
 *
 * The operation system may react to this e.g. with scanning the device for
 * partitions or creating device nodes for the user.
 * \endenglish
 */
void cdi_storage_device_init(struct cdi_storage_device* device)
{
	device->dev.bus_data = malloc(sizeof(struct cdi_bus_data));
	device->dev.bus_data->bus_type = CDI_STORAGE;
	vfs_RegisterDevice((struct cdi_device*)device);
}
