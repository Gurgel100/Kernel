/*
 * Copyright (c) 2007-2009 The tyndur Project. All rights reserved.
 *
 * This code is derived from software contributed to the tyndur Project
 * by Antoine Kaufmann.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _ATA_DEVICE_H_
#define _ATA_DEVICE_H_

#undef DEBUG_ENABLE

#include <stdint.h>

#include "cdi.h"
#include "cdi/storage.h"
#include "cdi/io.h"
#include "cdi/lists.h"
#include "cdi/scsi.h"

#define ATAPI_ENABLE

#define PCI_CLASS_ATA           0x01
#define PCI_SUBCLASS_ATA        0x01
#define PCI_VENDOR_VIA          0x1106

#define ATA_PRIMARY_CMD_BASE    0x1F0
#define ATA_PRIMARY_CTL_BASE    0x3F6
#define ATA_PRIMARY_IRQ         14

#define ATA_SECONDARY_CMD_BASE  0x170
#define ATA_SECONDARY_CTL_BASE  0x376
#define ATA_SECONDARY_IRQ       15

#define ATA_DELAY(c) do { ata_reg_inb(c, REG_ALT_STATUS); \
    ata_reg_inb(c, REG_ALT_STATUS);\
    ata_reg_inb(c, REG_ALT_STATUS); } while (0)

// Hinweis: Worst Case nach ATA-Spec waeren 30 Sekunden
#define ATA_IDENTIFY_TIMEOUT    5000

// So lange wird gewartet, bis ein Geraet bereit ist, wenn ein Befehl
// ausgefuehrt werden soll
#define ATA_READY_TIMEOUT       500

// Timeout beim Warten auf einen IRQ
#define ATA_IRQ_TIMEOUT         500

// Normale Sektorgroesse
#define ATA_SECTOR_SIZE         512

// Diese Register werden ueber die port_cmd_base angesprochen
#define REG_DATA                0x0
#define REG_ERROR               0x1
#define REG_FEATURES            0x1
#define REG_SEC_CNT             0x2
#define REG_LBA_LOW             0x3
#define REG_LBA_MID             0x4
#define REG_LBA_HIG             0x5
#define REG_DEVICE              0x6
#define REG_STATUS              0x7
#define REG_COMMAND             0x7

// Diese Register werden ueber port_ctl_base angesprochen. Das wird den
// Funktionen fuer den Registerzugriff mit den 0x10 mitgeteilt.
#define REG_CONTROL             0x10
#define REG_ALT_STATUS          0x10


// Device Register
#define DEVICE_DEV(x)           (x << 4)

// Status Register
#define STATUS_BSY              (1 << 7)
#define STATUS_DRDY             (1 << 6)
#define STATUS_DF               (1 << 5)
#define STATUS_DRQ              (1 << 3)
#define STATUS_ERR              (1 << 0)
#define STATUS_MASK             (STATUS_BSY | STATUS_DRDY | STATUS_DF | \
                                STATUS_DRQ | STATUS_ERR)

// Befehle
#define COMMAND_IDENTIFY        0xEC

// Control Register
#define CONTROL_HOB             (1 << 7)
#define CONTROL_SRST            (1 << 2)
#define CONTROL_NIEN            (1 << 1)


// Busmaster-Register (in port_bmr_base)
#define BMR_COMMAND             0x0
#define BMR_STATUS              0x2
#define BMR_PRDT                0x4

// Bits im Busmaster Command Register
#define BMR_CMD_START           (1 << 0)
#define BMR_CMD_READ            (1 << 3)

// Bits im Busmaster Status Register
#define BMR_STATUS_ERROR        (1 << 1)
#define BMR_STATUS_IRQ          (1 << 2)

/// Maximale Groesse eines DMA-Transfers
#define ATA_DMA_MAXSIZE         (64 * 1024)


// Debug
#ifdef DEBUG_ENABLE
    #define DEBUG(fmt, ...) printf("ata: " fmt, ## __VA_ARGS__)
#else
    #define DEBUG(...)
#endif

/**
 * Resultat beim IDENTIFY DEVICE Befehl
 */
struct ata_identfiy_data {
    struct {
        uint8_t                         : 7;
        uint8_t     removable           : 1;
        uint8_t                         : 7;
        uint8_t     ata                 : 1;
    } __attribute__((packed)) general_config;

    uint16_t        log_cyl;                    // Veraltet
    uint16_t                            : 16;
    uint16_t        log_heads;                  // Veraltet
    uint16_t                            : 16;
    uint16_t                            : 16;
    uint16_t        log_spt;                    // Veraltet
    uint16_t                            : 16;
    // 8
    uint16_t                            : 16;
    uint16_t                            : 16;
    char            serial_number[20];
    // 20
    uint16_t                            : 16;
    uint16_t                            : 16;
    uint16_t                            : 16;
    char            firmware_revision[8];
    // 27
    char            model_number[40];
    uint8_t                             : 8; // Immer 0x80
    uint8_t         max_sec_per_irq;
    // 48
    uint16_t                            : 16;
    struct {
        uint16_t                        : 8;
        uint8_t     dma                 : 1;
        uint8_t     lba                 : 1;
        uint8_t     iordy_disabled      : 1;
        uint8_t     irdy_supported      : 1;
        uint8_t                         : 1;
        uint8_t                         : 1;    // Irgendwas mit Standbytimer
        uint8_t                         : 2;
    } __attribute__((packed)) capabilities;
    uint16_t                            : 16; // Noch ein paar Faehigkeiten,
                                              // die wir aber sicher nicht
                                              // brauchen
    uint8_t                             : 8;
    uint8_t         pio_mode_number;
    uint16_t                            : 16;
    struct {
        uint8_t     current_chs         : 1;    // Words 54-56
        uint8_t     transfer_settings   : 1;    // Words 64-70
        uint8_t     ultra_dma           : 1;    // Word 88
        uint16_t                        : 13;
    } __attribute__((packed)) valid_words;
    uint16_t        cur_log_cyl;                // Veraltet
    uint16_t        cur_log_heads;              // Veraltet
    // 56
    uint16_t        cur_log_spt;                // Veraltet
    uint16_t        chs_capacity[2];            // Veraltet
    struct {
        uint8_t     cur_sec_per_int     : 8;
        uint8_t     setting_valid       : 1;
        uint8_t                         : 7;
    } __attribute__((packed)) multi_sector;
    uint32_t        lba_sector_count;           // LBA28
    uint16_t                            : 16;
    struct {
        uint8_t                         : 1;
        uint8_t     mode0_supported     : 1;
        uint8_t     mode1_supported     : 1;
        uint8_t     mode2_supported     : 1;
        uint8_t                         : 4;
        uint8_t     mode0_selected      : 1;
        uint8_t     mode1_selected      : 1;
        uint8_t     mode2_selected      : 1;
        uint8_t                         : 5;
    } __attribute__((packed)) multiword_dma;
    // 64
    uint8_t         pio_modes_supported;
    uint8_t                             : 8;
    // Multiword dma Zeiten
    uint16_t        mwd_time1;
    uint16_t        mwd_time2;
    uint16_t        mwd_time3;
    uint16_t        mwd_time4;
    uint16_t                            : 16;
    uint16_t                            : 16;
    uint16_t                            : 16;
    // 72
    uint16_t                            : 16;
    uint16_t                            : 16;
    uint16_t                            : 16;
    struct {
        uint8_t     max_depth           : 5;
        uint16_t                        : 11;
    } __attribute__((packed)) queue_depth;
    uint16_t                            : 16;
    uint16_t                            : 16;
    uint16_t                            : 16;
    uint16_t                            : 16;
    // 80
    union {
        uint16_t    raw;
        struct {
            uint8_t                     : 1;
            uint8_t ata1                : 1;
            uint8_t ata2                : 1;
            uint8_t ata3                : 1;
            uint8_t ata4                : 1;
            uint8_t ata5                : 1;
            uint8_t ata6                : 1;
            uint8_t ata7                : 1;
            uint16_t                    : 8;
        } __attribute__((packed)) bits;
    } major_version;
    uint16_t        minor_version;

    union {
        uint16_t    raw[2];
        struct {
            // Word 82
            uint8_t smart               : 1;
            uint8_t security_mode       : 1;
            uint8_t removable_media     : 1;
            uint8_t power_management    : 1;
            uint8_t packet              : 1;
            uint8_t write_cache         : 1;
            uint8_t look_ahead          : 1;
            uint8_t release_int         : 1;
            uint8_t service_int         : 1;
            uint8_t device_reset        : 1;
            uint8_t hpa                 : 1; // Host protected area
            uint8_t                     : 1;
            uint8_t write_buffer        : 1;
            uint8_t read_buffer         : 1;
            uint8_t nop                 : 1;
            uint8_t                     : 1;

            // Word 83
            uint8_t download_microcode  : 1;
            uint8_t rw_dma_queued       : 1;
            uint8_t cfa                 : 1;
            uint8_t apm                 : 1; // Advanced power management
            uint8_t removable_media_sn  : 1; // R. Media Status notification
            uint8_t power_up_standby    : 1;
            uint8_t set_features_spinup : 1;
            uint8_t                     : 1;
            uint8_t set_max_security    : 1;
            uint8_t auto_acoustic_mngmnt: 1; // Automatic acoustic management
            uint8_t lba48               : 1;
            uint8_t dev_config_overlay  : 1;
            uint8_t flush_cache         : 1;
            uint8_t flush_cache_ext     : 1;
            uint8_t                     : 2;
        } __attribute__((packed)) bits;
    } features_support;
    // 83
    uint16_t        todo[17];
    // 100
    uint64_t        max_lba48_address;
    // 107
    uint16_t        todo2[153];
} __attribute__((packed));

struct ata_partition {
    union {
    struct cdi_storage_device   storage;
        struct cdi_scsi_device      scsi;
    } dev;

    // Muss auf NULL gesetzt werden
    void*                       null;
    struct ata_device*          realdev;

    // Startsektor
    uint32_t                    start;
};

struct ata_device {
    union {
    struct cdi_storage_device   storage;
        struct cdi_scsi_device      scsi;
    } dev;

    // In der Partitionstruktur ist dieses Feld null, um Partitionen erkennen
    // zu koennen.
    struct ata_controller*      controller;
    
    // Liste mit den Partitionen
    cdi_list_t                  partition_list;

    uint8_t                     id;

    // 1 wenn es sich um ein ATAPI-Gerat handelt, 0 sonst
    // Bei ATAPI-Geraeten wir im union dev scsi verwendet, ansonsten storage
    uint8_t                     atapi;

    // 1 Wenn das Geraet lba48 unterstuetzt
    uint8_t                     lba48;

    // 1 Wenn das Geraet lba28 unterstuetzt
    uint8_t                     lba28;

    /// 1 wenn das Geraet DMA unterstuetzt
    uint8_t                     dma;

    // Funktionen fuer den Zugriff auf dieses Geraet
    int (*read_sectors) (struct ata_device* dev, uint64_t start, size_t count,
        void* dest);
    int (*write_sectors) (struct ata_device* dev, uint64_t start,
        size_t count, void* dest);
};

struct ata_controller {
    struct cdi_storage_driver   *storage;
    struct cdi_scsi_driver      *scsi;

    uint8_t                     id;
    uint16_t                    port_cmd_base;
    uint16_t                    port_ctl_base;
    uint16_t                    port_bmr_base;
    uint16_t                    irq;

    // Wird auf 1 gesetzt wenn IRQs benutzt werden sollen, also das NIEN-Bit im
    // Control register nicht aktiviert ist.
    int                         irq_use;
    /// Wird auf 1 gesetzt wenn DMA benutzt werden darf
    int                         dma_use;
    // HACKKK ;-)
    struct ata_device           irq_dev;


    /// Physische Adresse der Physical Region Descriptor Table (fuer DMA)
    uintptr_t                   prdt_phys;
    /// Virtuelle Adresse der Physical Region Descriptor Table (fuer DMA)
    uint64_t*                   prdt_virt;
    /// Physische Adresse des DMA-Puffers
    uintptr_t                   dma_buf_phys;
    /// Virtuelle Adresse des DMA-Puffers
    void*                       dma_buf_virt;
};

struct ata_request {
    struct ata_device* dev;
    
    enum {
        NON_DATA,
        PIO,
        DMA,
    } protocol;

    // Flags fuer die Uebertragung
    struct {
        enum {
            READ,
            WRITE
        } direction;
        
        // 1 fuer Polling; 0 fuer Interrupts
        uint8_t poll;
        
        // 1 fuer ATAPI; 0 fuer ata
        uint8_t ata;
        
        // 1 Wenn das LBA-Bit im Geraeteregister
        uint8_t lba;
    } flags;

    union {
        // Registersatz fuer ATA-Operationen
        struct {
            enum {
                IDENTIFY_DEVICE = 0xEC,
                IDENTIFY_PACKET_DEVICE = 0xA1,
                PACKET = 0xA0,
                READ_SECTORS = 0x20,
                READ_SECTORS_DMA = 0xC8,
                SET_FEATURES = 0xEF,
                WRITE_SECTORS = 0x30,
                WRITE_SECTORS_DMA = 0xCA,
            } command;
            uint8_t count;
            uint64_t lba;
            uint8_t features;
        } ata;
    } registers;

    // Anzahl der Blocks die uebertragen werden sollen
    uint16_t block_count;
    
    // Groesse eines Blocks
    uint16_t block_size;

    // Anzahl der schon verarbeitetn Blocks
    uint16_t blocks_done;

    // Puffer in den die Daten geschrieben werden sollen/aus dem sie gelesen
    // werden sollen.
    void* buffer;
    
    // Moegliche Fehler
    enum {
        NO_ERROR = 0,
        DEVICE_READY_TIMEOUT,
        IRQ_TIMEOUT

    } error;
};

void ata_init_controller(struct ata_controller* controller);
void ata_remove_controller(struct ata_controller* controller);
void ata_init_device(struct ata_device* dev);
void ata_remove_device(struct cdi_device* device);
int ata_read_blocks(struct cdi_storage_device* device, uint64_t block,
    uint64_t count, void* buffer);
int ata_write_blocks(struct cdi_storage_device* device, uint64_t block,
    uint64_t count, void* buffer);


// Einen ATA-Request absenden und ausfuehren
int ata_request(struct ata_request* request);

int ata_protocol_pio_out(struct ata_request* request);
int ata_protocol_pio_in(struct ata_request* request);

// ATA-Funktionen
int ata_drv_identify(struct ata_device* dev);
int ata_drv_read_sectors(struct ata_device* dev, uint64_t start, size_t count,
    void* buffer);
int ata_drv_write_sectors(struct ata_device* dev, uint64_t start, size_t count,
    void* buffer);

// ATAPI-Funktionen
int atapi_drv_identify(struct ata_device* dev);
void atapi_init_device(struct ata_device* device);
void atapi_remove_device(struct cdi_device* device);
int atapi_request(struct cdi_scsi_device* dev,struct cdi_scsi_packet* packet);

// Auf einen IRQ warten
int ata_wait_irq(struct ata_controller* controller, uint32_t timeout);



/**
 * Basis fuer ein bestimmtes Register ausfindig machen
 */
static inline uint16_t ata_reg_base(struct ata_controller* controller,
    uint8_t reg)
{
    // Fuer alle Register die ueber die ctl_base angesprochen werden muessen
    // setzen wir Bit 4.
    if ((reg & 0x10) == 0) {
        return controller->port_cmd_base;
    } else {
        return controller->port_ctl_base;
    }
}

/**
 * Byte aus Kontrollerregister lesen
 */
static inline uint8_t ata_reg_inb(struct ata_controller* controller,
    uint8_t reg)
{
    uint16_t base = ata_reg_base(controller, reg);
    return cdi_inb(base + (reg & 0xF));
}

/**
 * Byte in Kontrollerregister schreiben
 */
static inline void ata_reg_outb(struct ata_controller* controller, 
    uint8_t reg, uint8_t value)
{
    uint16_t base = ata_reg_base(controller, reg);
    cdi_outb(base + (reg & 0xF), value);
}

/**
 * Word aus Kontrollerregister lesen
 */
static inline uint16_t ata_reg_inw(struct ata_controller* controller,
    uint8_t reg)
{
    uint16_t base = ata_reg_base(controller, reg);
    return cdi_inw(base + (reg & 0xF));
}

/**
 * Byte in Kontrollerregister schreiben
 */
static inline void ata_reg_outw(struct ata_controller* controller,
    uint8_t reg, uint16_t value)
{
    uint16_t base = ata_reg_base(controller, reg);
    cdi_outw(base + (reg & 0xF), value);
}

/**
 * Geraet auwaehlen
 */
static inline void ata_drv_select(struct ata_device* dev)
{
    ata_reg_outb(dev->controller, REG_DEVICE, DEVICE_DEV(dev->id));
    ATA_DELAY(dev->controller);
}

/**
 * Mehrere Words von einem Port einlesen
 *
 * @param port   Portnummer
 * @param buffer Puffer in dem die Words abgelegt werden sollen
 * @param count  Anzahl der Words
 */
static inline void ata_insw(uint16_t port, void* buffer, uint32_t count)
{
    asm volatile("rep insw" : "+D"(buffer), "+c"(count) : "d"(port) : "memory");
}

/**
 * Mehrere Words auf einen Port schreiben
 *
 * @param port   Portnummer
 * @param buffer Puffer aus dem die Words gelesen werden sollen
 * @param count  Anzahl der Words
 */
static inline void ata_outsw(uint16_t port, void* buffer, uint32_t count)
{
    asm volatile("rep outsw" : "+S"(buffer), "+c"(count) : "d"(port) : "memory");
}

#endif
