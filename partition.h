/*
 * partition.h
 *
 *  Created on: 06.08.2014
 *      Author: pascal
 */

#ifdef BUILD_KERNEL

#ifndef PARTITION_H_
#define PARTITION_H_

#include "cdi.h"
#include "devicemng.h"
#include "stdint.h"

typedef enum{
	PART_TYPE_NONE = 0x00,
	PART_TYPE_LINUX = 0x83,
	PART_TYPE_ISO9660 = 0xCD,
	PART_TYPE_LAST = 0xFF
}__attribute__((packed))part_type_t;

typedef struct{
	struct{
		uint8_t Bootflag;
		uint8_t Head;
		uint8_t Sector : 6;
		uint8_t CylinderHi : 2;
		uint8_t CylinderLo;
		part_type_t Type;
		uint8_t lastHead;
		uint8_t lastSector : 6;
		uint8_t lastCylinderHi : 2;
		uint8_t lastCylinderLo;
		uint32_t firstLBA;
		uint32_t Length;
	}__attribute__((packed))entry[4];
}__attribute__((packed))PartitionTable_t;

typedef struct{
	uint8_t sector;
	uint8_t head;
	uint8_t cylinder;
}chs_t;

typedef struct{
	uint64_t id;
	char *name;
	vfs_device_t *vfs_dev;
	vfs_file_t dev_stream;
	struct cdi_fs_driver *fs_driver;
	vfs_filesystem_t *fs;
	size_t lbaStart, lbaSize, blocksize;
	part_type_t type;
}partition_t;

int partition_getPartitions(const char *dev_name, vfs_file_t dev_stream, void(*partition_callback)(void *context, void*), void *context);

#endif /* PARTITION_H_ */

#endif
