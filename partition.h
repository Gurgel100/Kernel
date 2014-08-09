/*
 * partition.h
 *
 *  Created on: 06.08.2014
 *      Author: pascal
 */

#ifndef PARTITION_H_
#define PARTITION_H_

#include "cdi.h"
#include "devicemng.h"
#include "stdint.h"

typedef enum{
	PART_TYPE_NONE = 0x00,
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
		uint8_t lastSector : 5;
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
	struct cdi_device *dev;
	struct cdi_fs_filesystem *fs;
	size_t lbaStart;
	size_t size;
	part_type_t type;
}partition_t;

int partition_getPartitions(device_t *dev);

#endif /* PARTITION_H_ */
