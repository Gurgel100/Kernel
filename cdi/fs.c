/*
 * fs.c
 *
 *  Created on: 13.08.2013
 *      Author: pascal
 */

#include "fs.h"
#include "vfs.h"

/**
 * Dateisystemtreiber-Struktur initialisieren
 *
 * @param driver Zeiger auf den Treiber
 */
void cdi_fs_driver_init(struct cdi_fs_driver* driver)
{
	driver->drv.type = CDI_FILESYSTEM;
	cdi_driver_init((struct cdi_driver*)driver);
}

/**
 * Dateisystemtreiber-Struktur zerstoeren
 *
 * @param driver Zeiger auf den Treiber
 */
void cdi_fs_driver_destroy(struct cdi_fs_driver* driver)
{
	cdi_driver_destroy((struct cdi_driver*)driver);
}

/**
 * Quelldateien fuer ein Dateisystem lesen
 * XXX Brauchen wir hier auch noch irgendwas errno-Maessiges?
 *
 * @param fs Pointer auf die FS-Struktur des Dateisystems
 * @param start Position von der an gelesen werden soll
 * @param size Groesse des zu lesenden Datenblocks
 * @param buffer Puffer in dem die Daten abgelegt werden sollen
 *
 * @return die Anzahl der gelesenen Bytes
 */
size_t cdi_fs_data_read(struct cdi_fs_filesystem* fs, uint64_t start,
    size_t size, void* buffer)
{
	vfs_Read(NULL, fs->osdep.fp, start, size, buffer);
	return size;
}
