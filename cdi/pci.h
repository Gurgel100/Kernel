/*
 * Copyright (c) 2007-2010 Kevin Wolf
 *
 * This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://sam.zoy.org/projects/COPYING.WTFPL for more details.
 */

#ifndef _CDI_PCI_H_
#define _CDI_PCI_H_

#include <stdint.h>

#include <cdi.h>
#include <cdi-osdep.h>
#include <cdi/lists.h>

#define PCI_CLASS_STORAGE         0x01
#define PCI_SUBCLASS_ST_SATA      0x06

#define PCI_CLASS_MULTIMEDIA      0x04
#define PCI_SUBCLASS_MM_HDAUDIO   0x03


/**
 * \german
 * Beschreibt ein PCI-Gerät
 * \endgerman
 * \english
 * Describes a PCI device
 * \endenglish
 */
struct cdi_pci_device {
    struct cdi_bus_data bus_data;

    uint16_t    bus;
    uint16_t    dev;
    uint16_t    function;

    uint16_t    vendor_id;
    uint16_t    device_id;

    uint8_t     class_id;
    uint8_t     subclass_id;
    uint8_t     interface_id;

    uint8_t     rev_id;

    uint8_t     irq;

    /**
     * \german
     * Liste von I/O-Ressourcen, die zum Gerät gehören (Inhalt der BARs,
     * struct cdi_pci_resource*)
     * \endgerman
     * \english
     * List of I/O resources which belong to the device (content of the BARs,
     * struct cdi_pci_ressource*)
     * \endenglish
     */
    cdi_list_t resources;

    cdi_pci_device_osdep meta;
};

/**
 * \german
 * Art der von einem BAR beschriebenen Ressource
 * \endgerman
 * \english
 * Type of the resource described by a BAR
 * \endenglish
 */
typedef enum {
    CDI_PCI_MEMORY,
    CDI_PCI_IOPORTS
} cdi_res_t;

/**
 * \german
 * Beschreibt eine I/O-Ressource eines Geräts (Inhalt eines BARs)
 * \endgerman
 * \english
 * Describes an I/O resource of a device (represents a BAR)
 * \endenglish
 */
struct cdi_pci_resource {
    /**
     * \german
     * Typ der Ressource (Speicher oder Ports)
     * \endgerman
     * \english
     * Type of the resource (memory or ports)
     * \endenglish
     */
    cdi_res_t    type;

    /**
     * \german
     * Basisadresse der Ressource (physische Speicheradresse oder Portnummer)
     * \endgerman
     * \english
     * Base address of the resource (physical memory address or port number)
     * \endenglish
     */
    uintptr_t    start;

    /**
     * \german
     * Größe der Ressource in Bytes
     * \endgerman
     * \english
     * Size of the ressource in bytes
     * \endenglish
     */
    size_t       length;

    /**
     * \german
     * Index des zur Ressource gehörenden BAR, beginnend bei 0
     * \endgerman
     * \english
     * Index of the BAR that belongs to the resource, starting with 0
     * \endenglish
     */
    unsigned int index;

    /**
     * \german
     * Virtuelle Adresse des gemappten MMIO-Speichers (wird von
     * cdi_pci_alloc_memory gesetzt)
     * \endgerman
     * \english
     * Virtual address of the mapped MMIO memory (is set by
     * cdi_pci_alloc_memory)
     * \endenglish
     */
    void*        address;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * \german
 * Gibt alle PCI-Geräte im System zurück. Die Geräte (struct cdi_pci_device*)
 * werden dazu in die übergebene Liste eingefügt.
 * \endgerman
 * \english
 * Queries all PCI devices in the machine. The devices (struct cdi_pci_device*)
 * are inserted into a given list.
 * \endenglish
 */
void cdi_pci_get_all_devices(cdi_list_t list);

/**
 * \german
 * Gibt die Information zu einem PCI-Gerät frei
 * \endgerman
 * \english
 * Frees the information for a PCI device
 * \endenglish
 */
void cdi_pci_device_destroy(struct cdi_pci_device* device);

/**
 * \german
 * Reserviert die IO-Ports des PCI-Geräts für den Treiber
 * \endgerman
 * \english
 * Allocates the IO ports of the PCI device for the driver
 * \endenglish
 */
void cdi_pci_alloc_ioports(struct cdi_pci_device* device);

/**
 * \german
 * Gibt die IO-Ports des PCI-Geräts frei
 * \endgerman
 * \english
 * Frees the IO ports of the PCI device
 * \endenglish
 */
void cdi_pci_free_ioports(struct cdi_pci_device* device);

/**
 * \german
 * Mappt den MMIO-Speicher des PCI-Geräts
 * \endgerman
 * \english
 * Maps the MMIO memory of the PCI device
 * \endenglish
 */
void cdi_pci_alloc_memory(struct cdi_pci_device* device);

/**
 * \german
 * Gibt den MMIO-Speicher des PCI-Geräts frei
 * \endgerman
 * \english
 * Frees the MMIO memory of the PCI device
 * \endenglish
 */
void cdi_pci_free_memory(struct cdi_pci_device* device);

/**
 * \if german
 * Signalisiert CDI-Treibern direkten Zugriff auf den PCI-Konfigurationsraum
 * \elseif english
 * Indicates direct access to the PCI configuration space to CDI drivers
 * \endif
 */
#define CDI_PCI_DIRECT_ACCESS

/**
 * \if german
 * Liest ein Byte (8 Bit) aus dem PCI-Konfigurationsraum eines PCI-Geraets
 *
 * @param device Das Geraet
 * @param offset Der Offset im Konfigurationsraum
 *
 * @return Der Wert an diesem Offset
 * \elseif english
 * Reads a byte (8 bit) from the PCI configuration space of a PCI device
 *
 * @param device The device
 * @param offset The offset in the configuration space
 *
 * @return The corresponding value
 * \endif
 */
uint8_t cdi_pci_config_readb(struct cdi_pci_device* device, uint8_t offset);

/**
 * \if german
 * Liest ein Word (16 Bit) aus dem PCI-Konfigurationsraum eines PCI-Geraets
 *
 * @param device Das Geraet
 * @param offset Der Offset im Konfigurationsraum
 *
 * @return Der Wert an diesem Offset
 * \elseif english
 * Reads a word (16 bit) from the PCI configuration space of a PCI device
 *
 * @param device The device
 * @param offset The offset in the configuration space
 *
 * @return The corresponding value
 * \endif
 */
uint16_t cdi_pci_config_readw(struct cdi_pci_device* device, uint8_t offset);

/**
 * \if german
 * Liest ein DWord (32 Bit) aus dem PCI-Konfigurationsraum eines PCI-Geraets
 *
 * @param device Das Geraet
 * @param offset Der Offset im Konfigurationsraum
 *
 * @return Der Wert an diesem Offset
 * \elseif english
 * Reads a dword (32 bit) from the PCI configuration space of a PCI device
 *
 * @param device The device
 * @param offset The offset in the configuration space
 *
 * @return The corresponding value
 * \endif
 */
uint32_t cdi_pci_config_readl(struct cdi_pci_device* device, uint8_t offset);

/**
 * \if german
 * Schreibt ein Byte (8 Bit) in den PCI-Konfigurationsraum eines PCI-Geraets
 *
 * @param device Das Geraet
 * @param offset Der Offset im Konfigurationsraum
 * @param value Der zu setzende Wert
 * \elseif english
 * Writes a byte (8 bit) into the PCI configuration space of a PCI device
 *
 * @param device The device
 * @param offset The offset in the configuration space
 * @param value Value to be set
 * \endif
 */
void cdi_pci_config_writeb(struct cdi_pci_device* device, uint8_t offset,
    uint8_t value);

/**
 * \if german
 * Schreibt ein Word (16 Bit) in den PCI-Konfigurationsraum eines PCI-Geraets
 *
 * @param device Das Geraet
 * @param offset Der Offset im Konfigurationsraum
 * @param value Der zu setzende Wert
 * \elseif english
 * Writes a word (16 bit) into the PCI configuration space of a PCI device
 *
 * @param device The device
 * @param offset The offset in the configuration space
 * @param value Value to be set
 * \endif
 */
void cdi_pci_config_writew(struct cdi_pci_device* device, uint8_t offset,
    uint16_t value);

/**
 * \if german
 * Schreibt ein DWord (32 Bit) in den PCI-Konfigurationsraum eines PCI-Geraets
 *
 * @param device Das Geraet
 * @param offset Der Offset im Konfigurationsraum
 * @param value Der zu setzende Wert
 * \elseif english
 * Writes a dword (32 bit) into the PCI configuration space of a PCI device
 *
 * @param device The device
 * @param offset The offset in the configuration space
 * @param value Value to be set
 * \endif
 */
void cdi_pci_config_writel(struct cdi_pci_device* device, uint8_t offset,
    uint32_t value);

#ifdef __cplusplus
}; // extern "C"
#endif

#endif

