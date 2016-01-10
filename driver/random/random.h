/*
 * random.h
 *
 *  Created on: 16.12.2015
 *      Author: pascal
 */

#ifndef RANDOM_H_
#define RANDOM_H_

#include "stdint.h"
#include "cdi/storage.h"

typedef uint64_t(*rand_func)();
typedef void(*seed_func)(uint64_t);

struct random_device {
	struct cdi_storage_device dev;
	rand_func rand;
	seed_func seed;
};

struct random_device *random_device_init(struct cdi_driver *driver, const char *name, rand_func rand, seed_func seed);
int random_read_blocks(struct cdi_storage_device *device, uint64_t block, uint64_t count, void *buffer);
int random_write_blocks(struct cdi_storage_device *device, uint64_t block, uint64_t count, void *buffer);

uint64_t read_rdrand();

#endif /* RANDOM_H_ */
