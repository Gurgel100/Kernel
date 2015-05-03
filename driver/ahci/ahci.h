/*
 * Copyright (c) 2013-2014 Kevin Wolf
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

#ifndef AHCI_H
#define AHCI_H

#include <stdint.h>

#include "cdi/storage.h"
#include "cdi/scsi.h"
#include "cdi/mem.h"

#define BIT(x) (1 << x)

#define MAX_PORTS 32
#define FIS_BYTES 256
#define CMD_LIST_BYTES 1024
#define CMD_TABLE_BYTES (sizeof(struct cmd_table) * sizeof(struct ahci_prd))

#define AHCI_IRQ_TIMEOUT 5000 /* ms */

enum {
    ATA_CMD_READ_DMA            = 0xc8,
    ATA_CMD_READ_DMA_EXT        = 0x25,
    ATA_CMD_WRITE_DMA           = 0xca,
    ATA_CMD_WRITE_DMA_EXT       = 0x35,
    ATA_CMD_PACKET              = 0xa0,
    ATA_CMD_IDENTIFY_DEVICE     = 0xec,
};

enum {
    REG_CAP     = 0x00, /* Host Capabilities */
    REG_GHC     = 0x04, /* Global Host Control */
    REG_IS      = 0x08, /* Interrupt Status */
    REG_PI      = 0x0c, /* Ports Implemented */

    REG_PORTS   = 0x100,

    PORTSIZE    = 0x80,
    REG_PxCLB   = 0x00, /* Command List Base Address */
    REG_PxCLBU  = 0x04, /* Command List Base Address Upper */
    REG_PxFB    = 0x08, /* FIS Base Address */
    REG_PxFBU   = 0x0c, /* FIS Base Address Upper */
    REG_PxIS    = 0x10, /* Interrupt Status */
    REG_PxIE    = 0x14, /* Interrupt Enable */
    REG_PxCMD   = 0x18, /* Command and status */
    REG_PxTFD   = 0x20, /* Task File Data*/
    REG_PxSIG   = 0x24, /* Signature */
    REG_PxSSTS  = 0x28, /* Serial ATA Status */
    REG_PxSCTL  = 0x2c, /* Serial ATA Control */
    REG_PxSERR  = 0x30, /* Serial ATA Error */
    REG_PxCI    = 0x38, /* Command Issue */
};

enum {
    CAP_NCS_SHIFT   = 8,
    CAP_NCS_MASK    = (0x1f << CAP_NCS_SHIFT),
};

enum {
    GHC_HR      = (1 <<  0), /* HBA Reset */
    GHC_IE      = (1 <<  1), /* Interrupt Enable */
    GHC_AE      = (1 << 31), /* AHCI Enable */
};

enum {
    PxIS_IFS    = (1 << 27), /* Interface Fatal Error Status */
    PxIS_HBDS   = (1 << 28), /* Host Bus Data Error Status */
    PxIS_HBFS   = (1 << 29), /* Host Bus Fatal Error Status */
    PxIS_TFES   = (1 << 30), /* Task File Error Status */
};

enum {
    PxCMD_ST    = (1 <<  0), /* Start */
    PxCMD_SUD   = (1 <<  1), /* Spin-Up Device */
    PxCMD_POD   = (1 <<  2), /* Power On Device */
    PxCMD_FRE   = (1 <<  4), /* FIS Receive Enable */
    PxCMD_FR    = (1 << 14), /* FIS Receive Running */
    PxCMD_CR    = (1 << 15), /* Command List Running */
    PxCMD_CPS   = (1 << 16), /* Cold Presence State */
    PxCMD_CPD   = (1 << 20), /* Cold Presence Detection */

    PxCMD_ICC_MASK      = (0xf << 28),
    PxCMD_ICC_ACTIVE    = (0x1 << 28),
};

enum {
    PxTFD_BSY   = (1 << 7), /* Interface is busy */
    PxTFD_DRQ   = (1 << 3), /* Data transfer is requested */
    PxTFD_ERR   = (1 << 0), /* Error during transfer */
};

enum {
    PxSSTS_DET_MASK     = (0xf << 0),
    PxSSTS_IPM_MASK     = (0xf << 8),

    PxSSTS_DET_PRESENT  = (0x3 << 0),
    PxSSTS_IPM_ACTIVE   = (0x1 << 8),
};

enum {
    PxSCTL_DET_MASK     = (0xf << 0),   /* Device Detection Initialization */
    PxSCTL_IPM_MASK     = (0xf << 8),   /* Interface Power Management
                                           Transitions Allowed */
    PxSCTL_IPM_NONE     = (0x3 << 8),
};

/* SATA 2.6: "11.3 Software reset protocol" */
enum {
    SATA_SIG_DISK       = 0x00000101,
    SATA_SIG_PACKET     = 0xeb140101,
    SATA_SIG_QEMU_CD    = 0xeb140000, /* Broken value in qemu < 2.2 */
};

struct ahci_port {
    struct cdi_mem_area*        fis;
    uint64_t                    fis_phys;

    struct cmd_header*          cmd_list;
    struct cdi_mem_area*        cmd_list_mem;
    uint64_t                    cmd_list_phys;

    /* TODO More than one command with one PRDT entry */
    struct cmd_table*           cmd_table;
    struct cdi_mem_area*        cmd_table_mem;
    uint64_t                    cmd_table_phys;

    uint32_t                    last_is;
};

struct ahci_device {
    struct cdi_device           dev;
    struct cdi_mem_area*        mmio;
    int                         irq;

    uint32_t                    ports;
    uint32_t                    cmd_slots;

    struct ahci_port            port[MAX_PORTS];
};

struct ahci_bus_data {
    struct cdi_bus_data         cdi;
    struct ahci_device*         ahci;
    int                         port;
};

struct ahci_disk {
    struct cdi_storage_device   storage;
    struct ahci_device*         ahci;
    int                         port;

    bool                        lba48;
};

struct ahci_atapi {
    struct cdi_scsi_device      scsi;
    struct ahci_disk            disk;
};

enum {
    FIS_TYPE_H2D            = 0x27,
    FIS_TYPE_D2H            = 0x34,
    FIS_TYPE_DMA_ACTIVATE   = 0x39,
    FIS_TYPE_DMA_SETUP      = 0x41,
    FIS_TYPE_DATA           = 0x46,
    FIS_TYPE_BIST_ACTIVATE  = 0x58,
    FIS_TYPE_PIO_SETUP      = 0x5f,
    FIS_TYPE_SET_DEV_BITS   = 0xa1,
};

struct received_fis {
    /* DMA Setup FIS */
    struct dsfis {
        uint8_t     type;
        uint8_t     flags;
        uint16_t    reserved;
        uint32_t    dma_buf_low;
        uint32_t    dma_buf_high;
        uint32_t    reserved_2;
        uint32_t    dma_buf_offset;
        uint32_t    dma_count;
        uint32_t    reserved_3;
    } dsfis;
    uint32_t padding_1;

    /* PIO Setup FIS */
    struct psfis {
        uint8_t     type;
        uint8_t     flags;
        uint8_t     status;
        uint8_t     error;
        uint8_t     lba_low;
        uint8_t     lba_mid;
        uint8_t     lba_high;
        uint8_t     device;
        uint8_t     lba_low_exp;
        uint8_t     lba_mid_exp;
        uint8_t     lba_high_exp;
        uint8_t     reserved;
        uint16_t    sector_count;
        uint8_t     reserved_2;
        uint8_t     e_status;
        uint16_t    xfer_count;
        uint16_t    reserved_3;
    } psfis;
    uint32_t padding_2[3];

    /* D2H Register FIS */
    struct rfis {
        uint8_t     type;
        uint8_t     flags;
        uint8_t     status;
        uint8_t     error;
        uint8_t     lba_low;
        uint8_t     lba_mid;
        uint8_t     lba_high;
        uint8_t     device;
        uint8_t     lba_low_exp;
        uint8_t     lba_mid_exp;
        uint8_t     lba_high_exp;
        uint8_t     reserved;
        uint16_t    sector_count;
        uint16_t    reserved_2[3];
    } rfis;
    uint32_t padding_3;

    /* Set Device Bits FIS */
    struct sdbfis {
        uint8_t     type;
        uint8_t     flags;
        uint8_t     status;
        uint8_t     error;
        uint32_t    reserved;
    } sdbfis;

    /* Unknown FIS */
    uint8_t ufis[64];
} __attribute__((packed));
CDI_BUILD_BUG_ON(sizeof(struct received_fis) != 0xa0);

enum {
    H2D_FIS_F_COMMAND = 0x80,
};

struct h2d_fis {
    uint8_t     type;
    uint8_t     flags;
    uint8_t     command;
    uint8_t     features;
    uint8_t     lba_low;
    uint8_t     lba_mid;
    uint8_t     lba_high;
    uint8_t     device;
    uint8_t     lba_low_exp;
    uint8_t     lba_mid_exp;
    uint8_t     lba_high_exp;
    uint8_t     features_exp;
    uint16_t    sector_count;
    uint8_t     reserved;
    uint8_t     control;
    uint32_t    reserved_2;
} __attribute__((packed));

struct ahci_prd {
    uint32_t    dba;        /* Data Base Address */
    uint32_t    dbau;       /* Data Base Address Upper */
    uint32_t    reserved;
    uint32_t    dbc;        /* Data Byte Count */
} __attribute__((packed));

enum {
    CMD_HEADER_F_FIS_LENGTH_5_DW    = (5 << 0),
    CMD_HEADER_F_WRITE              = (1 << 6),
    CMD_HEADER_F_ATAPI              = (1 << 5),
};

struct cmd_header {
    uint16_t    flags;
    uint16_t    prdtl;      /* PRDT Length (in entries) */
    uint32_t    prdbc;      /* PRD Byte Count */
    uint32_t    ctba0;      /* Command Table Base Address */
    uint32_t    ctba_u0;    /* Command Table Base Address (Upper 32 Bits) */
    uint64_t    reserved1;
    uint64_t    reserved2;
} __attribute__((packed));

struct cmd_table {
    struct h2d_fis  cfis;
    uint8_t         padding[0x40 - sizeof(struct h2d_fis)];
    uint8_t         acmd[0x10];         /* ATAPI command */
    uint8_t         reserved[0x30];
    struct ahci_prd prdt[];
} __attribute__((packed));

/* Register access functions */

static inline void reg_outl(struct ahci_device* ahci, int reg, uint32_t value)
{
    volatile uint32_t* mmio = (uint32_t*) ((uint8_t*) ahci->mmio->vaddr + reg);
    *mmio = value;
}

static inline uint32_t reg_inl(struct ahci_device* ahci, int reg)
{
    volatile uint32_t* mmio = (uint32_t*) ((uint8_t*) ahci->mmio->vaddr + reg);
    return *mmio;
}


static inline void pxreg_outl(struct ahci_device* ahci, int port, int reg,
                              uint32_t value)
{
    volatile uint32_t* mmio = (uint32_t*) ((uint8_t*) ahci->mmio->vaddr +
        REG_PORTS + port * PORTSIZE + reg);
    *mmio = value;
}

static inline uint32_t pxreg_inl(struct ahci_device* ahci, int port, int reg)
{
    volatile uint32_t* mmio = (uint32_t*) ((uint8_t*) ahci->mmio->vaddr +
        REG_PORTS + port * PORTSIZE + reg);
    return *mmio;
}

/* ahci/main.c */
void ahci_port_comreset(struct ahci_device* ahci, int port);

/* ahci/disk.c */
extern struct cdi_storage_driver ahci_disk_driver;
extern struct cdi_scsi_driver ahci_atapi_driver;


#endif
