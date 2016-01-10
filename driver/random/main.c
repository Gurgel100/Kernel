/*
 * main.c
 *
 *  Created on: 16.12.2015
 *      Author: pascal
 */

#include "cdi/storage.h"
#include "random.h"
#include "mt.h"
#include <stdbool.h>
#include "cpu.h"

#define DRIVER_NAME "random"

static struct cdi_storage_driver random_driver;

/*
 * Prüft, ob die Funktion rdrand verfügbar ist
 * Rückgabe:	0, wenn die Funktion nicht unterstützt wird
 * 				!0, wenn die Funktion unterstützt wird
 */
static bool check_rdrand()
{
	return cpuInfo.rdrand;
}

static int random_driver_init(void)
{
	struct random_device *dev;
	//Konstruktor der Vaterklasse
	cdi_storage_driver_init(&random_driver);

	if(check_rdrand())
	{
		dev = random_device_init((struct cdi_driver*)&random_driver, "rdrand", read_rdrand, NULL);
		cdi_list_push(random_driver.drv.devices, dev);
	}

	dev = random_device_init((struct cdi_driver*)&random_driver, "urandom", randomMT, (seed_func)seedMT);

	return 0;
}

static int random_driver_destroy(void)
{
	//Destruktor der Vaterklasse
	cdi_storage_driver_destroy(&random_driver);
	return 0;
}

static struct cdi_storage_driver random_driver = {
	.drv = {
		.type = CDI_STORAGE,
		.name = DRIVER_NAME,
		.init = random_driver_init,
		.destroy = random_driver_destroy
	},
	.read_blocks = random_read_blocks,
	.write_blocks = random_write_blocks
};

CDI_DRIVER(DRIVER_NAME, random_driver);
