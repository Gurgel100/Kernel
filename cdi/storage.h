/*
 * Copyright (c) 2007 Antoine Kaufmann
 *
 * This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://sam.zoy.org/projects/COPYING.WTFPL for more details.
 */

/**
 * \german
 * Das Modul Storage enthält Treiber fuer Massenspeichergeräte.
 *
 * Storage implementiert zwei grundlegende Datenstrukturen aus \ref core und
 * erweitert sie um Eigenschaften, die für Massenspeicher spezifisch sind:
 *
 * - cdi_storage_device beschreibt ein Massenspeichergerät.
 * - cdi_storage_driver enthält Funktionspointer aus die Funktionen eines
 *   Treibers, der Massenspeichergeräte zur Verfügung stellt.
 *
 * Treiber können die in storage.h definierten Funktionen aufrufen, um mit der
 * CDI-Implementierung zu kommunizieren.
 * \endgerman
 * \english
 * The Storage module contains drivers for mass storage devices.
 *
 * Storage implements two of the data structures defined in \ref core and
 * extends them by properties specific to mass storage devices:
 *
 * - cdi_storage_device describes a mass storage device
 * - cdi_storage_driver contains function pointers to the functions that a mass
 *   storage driver provides.
 *
 * Drivers may call the functions defined in storage.h in order to communicate
 * with the CDI implementation.
 * \endenglish
 * \defgroup storage
 */
/*\@{*/

#ifndef _CDI_STORAGE_H_
#define _CDI_STORAGE_H_

#include <stdint.h>

#include <cdi.h>

/**
 * \german
 * Repräsentiert ein Massenspeichergerät.
 * \endgerman
 * \english
 * Represents a mass storage device.
 * \endenglish
 * \extends cdi_device
 */
struct cdi_storage_device {
    struct cdi_device   dev;

    /**
     * \german
     * Blockgröße des Geräts
     * \endgerman
     * \english
     * Block size of this device
     * \endenglish
     */
    size_t              block_size;

    /**
     * \german
     * Größe des Massenspeichers in Blöcken
     * \endgerman
     * \english
     * Size of the mass storage device in blocks
     * \endenglish
     */
    uint64_t            block_count;
};

/**
 * \german
 * Beschreibt einen Treiber für Massenspeichergeräte.
 * \endgerman
 * \english
 * Describes a driver for mass storage devices.
 * \endenglish
 * \extends cdi_driver
 */
struct cdi_storage_driver {
    struct cdi_driver   drv;

    /**
     * \german
     * Liest Blöcke vom Gerät ein
     *
     * @param start Blocknummer des ersten zu lesenden Blockes (angefangen
     *              bei 0).
     * @param count Anzahl der zu lesenden Blocks
     * @param buffer Puffer in dem die Daten abgelegt werden sollen
     *
     * @return 0 bei Erfolg, -1 im Fehlerfall.
     * \endgerman
     * \english
     * Reads blocks from the device
     *
     * @param start Number of the first block to be read (first block on the
     * device is 0)
     * @param count Number of blocks to read
     * @param buffer Buffer in which the read data should be stored
     *
     * @return 0 on success, -1 in error cases
     * \endenglish
     */
    int (*read_blocks)(struct cdi_storage_device* device, uint64_t start,
        uint64_t count, void* buffer);

    /**
     * \german
     * Schreibt Blöcke auf das Gerät
     *
     * @param start Blocknummer des ersten zu schreibenden Blockes (angefangen
     *              bei 0).
     * @param count Anzahl der zu schreibenden Blocks
     * @param buffer Puffer aus dem die Daten gelesen werden sollen
     *
     * @return 0 bei Erfolg, -1 im Fehlerfall
     * \endgerman
     * \english
     * Writes blocks to the device
     *
     * @param start Number of the first block to be written (first block on the
     * device is 0)
     * @param count Number of blocks to write
     * @param buffer Buffer which contains the data to be written
     *
     * @return 0 on success, -1 in error cases
     * \endenglish
     */
    int (*write_blocks)(struct cdi_storage_device* device, uint64_t start,
        uint64_t count, void* buffer);
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \german
 * Initialisiert die Datenstrukturen fuer einen Massenspeichertreiber
 * (erzeugt die devices-Liste). Diese Funktion muss während der
 * Initialisierung eines Massenspeichertreibers aufgerufen werden.
 * \endgerman
 * \english
 * Initialises the data structures for a mass storage device. This function
 * must be called during initialisation of a mass storage driver.
 * \endenglish
 *
 */
void cdi_storage_driver_init(struct cdi_storage_driver* driver);

/**
 * \german
 * Deinitialisiert die Datenstrukturen fuer einen Massenspeichertreiber
 * (gibt die devices-Liste frei)
 * \endgerman
 * \english
 * Deinitialises the data structures for a mass storage driver
 * \endenglish
 */
void cdi_storage_driver_destroy(struct cdi_storage_driver* driver);

/**
 * \german
 * Meldet ein Massenspeichergerät beim Betriebssystem an. Diese Funktion muss
 * aufgerufen werden, nachdem ein Massenspeichertreiber alle notwendigen
 * Initialisierungen durchgeführt hat.
 *
 * Das Betriebssystem kann dann beispielsweise nach Partitionen auf dem Gerät
 * suchen und Gerätedateien verfügbar machen.
 * \endgerman
 * \english
 * Registers a mass storage device with the operating system. This function
 * must be called once a mass storage driver has completed all required
 * initialisations.
 *
 * The operation system may react to this e.g. with scanning the device for
 * partitions or creating device nodes for the user.
 * \endenglish
 */
void cdi_storage_device_init(struct cdi_storage_device* device);

#ifdef __cplusplus
}; // extern "C"
#endif

#endif

/*\@}*/

