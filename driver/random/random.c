/*
 * random.c
 *
 *  Created on: 16.12.2015
 *      Author: pascal
 */

#include "random.h"
#include <stdlib.h>
#include <string.h>
#include "cpu.h"

struct random_device *random_device_init(struct cdi_driver *driver, const char *name, rand_func rand, seed_func seed)
{
	struct random_device *dev = calloc(1, sizeof(struct random_device));
	if(dev != NULL)
	{
		dev->dev.dev.driver = driver;
		dev->dev.dev.name = strdup(name);
		dev->dev.block_size = sizeof(__typeof(rand_func));
		dev->dev.block_count = -1ul;
		dev->rand = rand;
		dev->seed = seed;

		cdi_storage_device_init(&dev->dev);
	}
	return dev;
}

int random_read_blocks(struct cdi_storage_device *device, uint64_t block __attribute__((unused)), uint64_t count, void *buffer)
{
	size_t i;
	struct random_device *dev = (struct random_device*)device;
	uint64_t *buf = (uint64_t*)buffer;

	for(i = 0; i < count; i++)
	{
		buf[i] = dev->rand();
	}

	return 0;
}

int random_write_blocks(struct cdi_storage_device *device, uint64_t block __attribute__((unused)), uint64_t count, void *buffer)
{
	struct random_device *dev = (struct random_device*)device;
	uint64_t *buf = (uint64_t*)buffer;

	if(dev->seed)
	{
		dev->seed(buf[count - 1]);
	}

	return 0;
}

asm("_prev_read_rdrand:"
	"pause;"
	".global read_rdrand;"
	"read_rdrand:"
	"rdrand %rax;"
	"jnc _prev_read_rdrand;"
	"retq;");
