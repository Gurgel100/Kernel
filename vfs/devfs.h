/*
 * devfs.h
 *
 *  Created on: 07.01.2018
 *      Author: pascal
 */

#ifndef DEVFS_H_
#define DEVFS_H_

#include "node.h"

void devfs_init();
int devfs_registerDeviceNode(vfs_node_dev_t *node);

#endif /* DEVFS_H_ */
