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

struct cdi_fs_filesystem *getFilesystem(partition_t *part);
struct cdi_fs_driver *getFSDriver(const char *name);

/*
 * Liest die Partitionstabelle aus.
 * Parameter:	dev = CDI-Gerätestruktur, das das Gerät beschreibt
 * Rückgabe:	!0 bei Fehler
 */
int partition_getPartitions(device_t *dev)
{
	//Ersten Sektor auslesen
	void *buffer = malloc(512);
	if(!dmng_Read(dev, 0, 512, buffer))
		return 1;

	//Partitionstabelle durchsuchen
	PartitionTable_t *ptable = buffer + 0x1BE;
	uint8_t i;
	dev->partitions = list_create();
	for(i = 0; i < 4; i++)
	{
		if(ptable->entry[i].Type)
		{
			partition_t *part = malloc(sizeof(partition_t));
			part->id = i + 1;
			part->lbaStart = ptable->entry[i].firstLBA;
			part->size = ptable->entry[i].Length;
			part->type = ptable->entry[i].Type;
			part->dev = dev->device;
			part->fs = getFilesystem(part);
			list_push(dev->partitions, part);
		}
	}
	return 0;
}

/*
 * Erzeugt die Dateisystemstruktur und füllt diese mit dem richtigen Treiber.
 * Parameter:	part = Partition, für die das Dateisystem gesucht werden soll
 * Rückgabe:	Zeiger auf Dateisystemstruktur oder NULL wenn kein passendes Dateisystem gefunden
 */
struct cdi_fs_filesystem *getFilesystem(partition_t *part)
{
	struct cdi_fs_filesystem *fs = malloc(sizeof(*fs));
	struct cdi_fs_driver *driver;

	switch(part->type)
	{
		case PART_TYPE_ISO9660:
			if(!(driver = getFSDriver("iso9660")))
				goto exit_error;
			fs->driver = driver;
			asprintf(&fs->osdep.devPath, "dev/%s", part->dev->name);
		break;
		default:
			goto exit_error;
	}

	return fs;

	exit_error:
	free(fs);
	return NULL;
}

/*
 * Sucht den Dateisystemtreiber mit dem angebenen Namen
 * Parameter:	name = Name des Dateisystemtreibers, nach dem gesucht werden soll
 * Rückgabe:	Zeiger auf Dateisystemtreiberstruktur oder NULL falls kein passender Treiber gefunden wurde
 */
struct cdi_fs_driver *getFSDriver(const char *name)
{
	struct cdi_driver *driver;

	size_t i = 0;
	extern drivers;
	while((driver = cdi_list_get(drivers, i)))
	{
		if(driver->type == CDI_FILESYSTEM && strcmp(name, driver->name) == 0)
			return (struct cdi_fs_driver*)driver;
		i++;
	}

	return NULL;
}
