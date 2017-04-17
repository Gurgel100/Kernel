/*
 * drivermanager.h
 *
 *  Created on: 05.04.2017
 *      Author: pascal
 */

#ifndef DRIVERMANAGER_H_
#define DRIVERMANAGER_H_

#include "cdi.h"

/**
 * \brief Initializes the driver manager.
 */
void drivermanager_init();

/**
 * \brief Registers a driver in the driver manager.
 *
 * @param drv Driver to be registered
 */
void drivermanager_registerDriver(struct cdi_driver *drv);

/**
 * \brief Gets the driver with the specified name.
 *
 * @param name Name of the driver
 * @return the driver with a matching name or NULL
 */
struct cdi_driver *drivermanager_getDriver(const char *name);

/**
 * \brief Gets the next suitable driver.
 *
 * @param type Type of the driver searched
 * @param prev Previous returned driver
 * @return a suitable driver or NULL
 */
struct cdi_driver *drivermanager_getNextDeviceDriver(cdi_device_type_t bus, struct cdi_driver *prev);

#endif /* DRIVERMANAGER_H_ */
