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

#define TYPE_NONE	0x0

typedef struct{
	struct{
		uint8_t Bootflag;
		uint8_t Head;
		uint8_t Sector : 6;
		uint8_t CylinderHi : 2;
		uint8_t CylinderLo;
		uint8_t Type;
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
	struct cdi_device *dev;
	struct cdi_fs_driver *fs;
	size_t lbaStart;
	size_t size;
}partition_t;

void partition_getPartitions(device_t *dev);

#endif /* PARTITION_H_ */
