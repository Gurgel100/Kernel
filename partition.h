/*
 * partition.h
 *
 *  Created on: 06.08.2014
 *      Author: pascal
 */

#ifndef PARTITION_H_
#define PARTITION_H_

#include "vfs.h"

int partition_getPartitions(const char *dev_name, vfs_file_t dev_stream, void(*partition_callback)(void *context, void*), void *context);

#endif /* PARTITION_H_ */
