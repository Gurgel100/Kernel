/*
 * Copyright (c) 2009 Kevin Wolf
 *
 * This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://sam.zoy.org/projects/COPYING.WTFPL for more details.
 */

#ifndef _CDI_OSDEP_H_
#define _CDI_OSDEP_H_

// Die folgenden Makros werden dazu benutzt, ein Array aller Treiber, die in
// der Binary vorhanden sind, zu erstellen (das Array besteht aus struct
// cdi_driver*).
//
// Um die Grenzen des Arrays zu kennen, werden Pointer auf die einzelnen
// Treiber in eine eigene Section cdi_drivers gelegt. Ueber Symbole am Anfang
// und Ende der Section kann die Bibliothek alle Treiber aufzaehlen.
//
// Die Variablennamen sind an sich unwichtig (der Wert wird nie ueber den
// Variablennamen abgefragt), aber sie muessen eindeutig sein. Sie werden daher
// static deklariert und innerhalb derselben Datei mit einer fortlaufenden
// Nummer unterschieden (__cdi_driver_0 usw.).
#define cdi_glue(x, y) x ## y
#define cdi_declare_driver(drv, counter) \
    static const void* __attribute__((section("cdi_drivers"), used)) \
        cdi_glue(__cdi_driver_, counter) = &drv;

/**
 * \german
 * Muss fuer jeden CDI-Treiber genau einmal verwendet werden, um ihn bei der
 * CDI-Bibliothek zu registrieren
 *
 * @param name Name des Treibers
 * @param drv Treiberbeschreibung (struct cdi_driver)
 * @param deps Liste von Namen anderer Treiber, von denen der Treiber abhaengt
 * \endgerman
 *
 * \english
 * CDI_DRIVER shall be used exactly once for each CDI driver. It registers
 * the driver with the CDI library.
 *
 * @param name Name of the driver
 * @param drv A driver description (struct cdi_driver)
 * @param deps List of names of other drivers on which this driver depends
 * \endenglish
 */
#define CDI_DRIVER(name, drv, deps...) cdi_declare_driver(drv, __COUNTER__)

/**
 * \german
 * OS-spezifische Daten zu PCI-Geraeten
 * \endgerman
 * \english
 * OS-specific PCI data.
 * \endenglish
 */
typedef struct
{
} cdi_pci_device_osdep;

/**
 * \german
 * OS-spezifische Daten fuer einen ISA-DMA-Zugriff
 * \endgerman
 * \english
 * OS-specific DMA data.
 * \endenglish
 */
typedef struct
{
} cdi_dma_osdep;

/**
 * \german
 * OS-spezifische Daten fuer Speicherbereiche
 * \endgerman
 * \english
 * OS-specific data for memory areas.
 * \endenglish
 */
typedef struct {
} cdi_mem_osdep;

/**
 * \german
 * OS-spezifische Daten fuer Dateisysteme
 * \endgerman
 * \english
 * OS-specific data for file systems
 * \endenglish
 */
typedef struct {
	char *devPath;
} cdi_fs_osdep;

#endif
