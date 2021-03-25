/*
 * partition.c
 *
 *  Created on: 06.08.2014
 *      Author: pascal
 */

#include "partition.h"
#include "storage.h"
#include "scsi.h"
#include "fs.h"
#include "lists.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "drivermanager.h"
#include "assert.h"
#include "devfs.h"

#define MIN(val1, val2) ((val1 < val2) ? val1 : val2)

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
	vfs_stream_t *dev_stream;
	vfs_filesystem_t *fs;
	size_t lbaStart, lbaSize, blocksize;
	part_type_t type;
}partition_t;

typedef struct{
	vfs_node_dev_t base;
	partition_t *partition;
}partition_vfs_node_dev_t;

/*
 * Liest Daten von einer Partition
 */
static size_t read(vfs_node_dev_t *node, uint64_t start, size_t size, void *buffer)
{
	partition_vfs_node_dev_t *pnode = (partition_vfs_node_dev_t*)node;
	partition_t *part = pnode->partition;
	uint64_t corrected_start = MIN(start, part->lbaSize * part->blocksize);
	return vfs_Read(part->dev_stream, part->lbaStart * part->blocksize + corrected_start, MIN(part->lbaSize * part->blocksize - corrected_start, size), buffer);
}

/*
 * Schreibt Daten auf eine Partition
 */
static size_t write(vfs_node_dev_t *node, uint64_t start, size_t size, const void *buffer)
{
	partition_vfs_node_dev_t *pnode = (partition_vfs_node_dev_t*)node;
	partition_t *part = pnode->partition;
	uint64_t corrected_start = MIN(start, part->lbaSize * part->blocksize);
	return vfs_Write(part->dev_stream, part->lbaStart * part->blocksize + corrected_start, MIN(part->lbaSize * part->blocksize - corrected_start, size), buffer);
}

/*
 * Gibt bestimmte Werte zurück, welche vom VFS verwendet werden
 */
static int getAttribute(vfs_node_t *node, vfs_fileinfo_t attribute, uint64_t *value)
{
	assert(node->type == VFS_NODE_DEV);
	partition_vfs_node_dev_t *pnode = (partition_vfs_node_dev_t*)node;
	partition_t *part = pnode->partition;

	switch(attribute)
	{
		case VFS_INFO_BLOCKSIZE:
			*value = part->blocksize;
		break;
		default:
			return 1;
	}

	return 0;
}

/*
 * Liest die Partitionstabelle aus.
 * Parameter:	dev = CDI-Gerätestruktur, das das Gerät beschreibt
 * Rückgabe:	!0 bei Fehler
 */
int partition_getPartitions(const char *dev_name, void(*partition_callback)(void *context, void*), void *context)
{
	char *path = NULL;
	asprintf(&path, ":devfs/%s", dev_name);
	vfs_stream_t *dev_stream = vfs_Open(path, VFS_MODE_READ);
	free(path);
	if(dev_stream == NULL)
		return 1;
	//Ersten Sektor auslesen
	void *buffer = malloc(512);
	if(vfs_Read(dev_stream, 0, 512, buffer) == 0)
	{
		vfs_Close(dev_stream);
		return 1;
	}

	//Gültige Partitionstabelle?
	uint16_t *sig = buffer + 0x1FE;
	if(*sig != 0xAA55)
	{
		vfs_Close(dev_stream);
		return -1;
	}

	//Partitionstabelle durchsuchen
	size_t foundPartitions = 0;
	PartitionTable_t *ptable = buffer + 0x1BE;
	uint8_t i;
	for(i = 0; i < 4; i++)
	{
		if(ptable->entry[i].Type)
		{
			partition_t *part = malloc(sizeof(partition_t));
			part->id = i + 1;
			asprintf(&part->name, "%s_%hhu", dev_name, i);
			part->lbaStart = ptable->entry[i].firstLBA;
			part->lbaSize = ptable->entry[i].Length;
			part->type = ptable->entry[i].Type;
			part->dev_stream = dev_stream;
			part->blocksize = vfs_getFileinfo(dev_stream, VFS_INFO_BLOCKSIZE);
			part->fs = NULL;
			if(part->type == PART_TYPE_NONE)
			{
				free(part->name);
				free(part);
				continue;
			}

			if(partition_callback != NULL)
				partition_callback(context, part);

			if(part->type == PART_TYPE_ISO9660)
			{
				//Korigiere Start und Grösse der Partition, weil das Dateisystem selber korrigiert
				//XXX: Vielleicht gibt es einen anderen Weg es besser zu machen
				part->lbaStart = 0;
				part->lbaSize = -1ul;
			}

			//register partition at the devfs
			partition_vfs_node_dev_t *node = calloc(1, sizeof(partition_vfs_node_dev_t));
			if(node == NULL || vfs_node_dev_init(&node->base, part->name))
				continue;	//TODO: maybe throw error?
			node->partition = part;
			node->base.type = VFS_DEVICE_STORAGE | VFS_DEVICE_MOUNTABLE;
			node->base.read = read;
			node->base.write = write;
			node->base.base.getAttribute = getAttribute;
			devfs_registerDeviceNode(&node->base);

			foundPartitions++;
		}
	}

	if(foundPartitions == 0)
		vfs_Close(dev_stream);

	return 0;
}
