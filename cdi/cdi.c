/*
 * cdi.c
 *
 *  Created on: 30.07.2013
 *      Author: pascal
 */

#include "cdi.h"
#include "isr.h"
#include "pci.h"

extern struct cdi_driver *__start_cdi_drivers;
extern struct cdi_driver *__stop_cdi_drivers;

cdi_list_t drivers;

/**
 * \german
 * Treiber, die ihre eigene main-Funktion implemenieren, müssen diese Funktoin
 * vor dem ersten Aufruf einer anderen CDI-Funktion aufrufen.
 * Initialisiert interne Datenstruktur der Implementierung fuer das jeweilige
 * Betriebssystem und startet anschliessend alle Treiber.
 *
 * Ein wiederholter Aufruf bleibt ohne Effekt. Es bleibt der Implementierung
 * der CDI-Bibliothek überlassen, ob diese Funktion zurückkehrt.
 * \endgerman
 *
 * \english
 * Drivers which implement their own main() function must call this function
 * before they call any other CDI function. It initialises internal data
 * structures of the CDI implementation and starts all drivers.
 *
 * This function should only be called once, additional calls will have no
 * effect. Depending on the implementation, this function may or may not
 * return.
 * \endenglish
 */
void cdi_init()
{
	struct cdi_driver **pdrv;
	struct cdi_driver *drv;

	extern cdi_list_t IRQHandlers;
	IRQHandlers = cdi_list_create();

	drivers = cdi_list_create();

	// Alle in dieser Binary verfügbaren Treiber aufsammeln
	for(pdrv = &__start_cdi_drivers; pdrv < &__stop_cdi_drivers; pdrv++)
	{
		drv = *pdrv;
		if(drv->init != NULL)
		{
			drv->init();
			cdi_driver_register(drv);
		}
	}

	//Gibt es PCI-Geräte für einen Treiber?
	cdi_list_t pci_devices = cdi_list_create();
	cdi_pci_get_all_devices(pci_devices);
	struct cdi_pci_device *pci;
	struct cdi_driver *driver;
	struct cdi_device *device;
	while(!(pci = cdi_list_pop(pci_devices)))
	{

		// I/O-Ports, MMIO und Busmastering aktivieren
		uint16_t val = cdi_pci_config_readw(pci, 4);
		cdi_pci_config_writew(pci, 4, val | 0x7);

		device = NULL;

		//Treiber suchen
		size_t j;
		for(j = 0; (driver = cdi_list_get(drivers, j)); j++)
		{
			if(driver->bus == CDI_PCI && driver->init_device)
			{
				device = driver->init_device(&pci->bus_data);
				break;
			}
		}
		if(device != NULL)
		{
			cdi_list_push(driver->devices, device);
		}
		else
		{
			cdi_pci_device_destroy(pci);
		}
	}
	cdi_list_destroy(pci_devices);
}

/**
 * \german
 * Initialisiert die Datenstrukturen für einen Treiber
 * (erzeugt die devices-Liste)
 * \endgerman
 *
 * \english
 * Initialises the data structures for a driver
 * \endenglish
 */
void cdi_driver_init(struct cdi_driver* driver)
{
	driver->devices = cdi_list_create();
}

/**
 * \german
 * Deinitialisiert die Datenstrukturen für einen Treiber
 * (gibt die devices-Liste frei)
 * \endgerman
 *
 * \english
 * Deinitialises the data structures for a driver
 * \endenglish
 */
void cdi_driver_destroy(struct cdi_driver* driver)
{
	cdi_list_destroy(driver->devices);
}

/**
 * \german
 * Registriert den Treiber fuer ein neues Geraet
 *
 * @param driver Zu registierender Treiber
 * \endgerman
 *
 * \english
 * Registers a new driver with CDI
 * \endenglish
 */
void cdi_driver_register(struct cdi_driver* driver)
{
	printf("Registered Driver: %s\n", driver->name);
	cdi_list_push(drivers, driver);
}
