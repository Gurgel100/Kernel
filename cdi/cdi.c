/*
 * cdi.c
 *
 *  Created on: 30.07.2013
 *      Author: pascal
 */

#include "cdi.h"
#include "isr.h"

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
