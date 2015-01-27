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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "cdi.h"
#include "cdi/misc.h"

#include "ahci.h"

#define DISK_DRIVER_NAME "ahci-disk"
#define ATAPI_DRIVER_NAME "ahci-cd"

/**
 * This function is called for any requests (even succeeding ones). Its job is
 * to recover from any failures and decide whether a request must return
 * failues.
 *
 * @return 0 if the command can still be completed successfully, -1 if failure
 * should be returned.
 */
static int ahci_request_handle_error(struct ahci_disk* disk, uint32_t is)
{
    /* AHCI 1.3: "6.2.2 Software Error Recovery" */
    if (is & (PxIS_HBFS | PxIS_HBDS | PxIS_IFS | PxIS_TFES)) {
        /* Fatal errors. Restart the port and return failure. */
        uint32_t cmd, tfd;

        /* Stor processing of the command queue */
        cmd = pxreg_inl(disk->ahci, disk->port, REG_PxCMD);
        pxreg_outl(disk->ahci, disk->port, REG_PxCMD, cmd & ~PxCMD_ST);
        while (pxreg_inl(disk->ahci, disk->port, REG_PxCMD) & PxCMD_CR);

        /* Reset SATA error register */
        pxreg_outl(disk->ahci, disk->port, REG_PxSERR, 0xffffffff);

        /* COMRESET if PxTFD.STS.(BSY|DRQ) == 1 */
        tfd = pxreg_inl(disk->ahci, disk->port, REG_PxTFD);
        if (tfd & (PxTFD_BSY | PxTFD_DRQ)) {
            ahci_port_comreset(disk->ahci, disk->port);
        }

        /* Restart port */
        pxreg_outl(disk->ahci, disk->port, REG_PxCMD, cmd | PxCMD_ST);
        return -1;
    }

    return 0;
}

static int ahci_request(struct ahci_disk* disk, int cmd, uint64_t lba,
                        uint64_t bytes, struct cdi_mem_area* buf, void* acmd)
{
    struct ahci_port *port = &disk->ahci->port[disk->port];
    uint32_t flags, device;

    if (buf->paddr.num != 1) {
        return -1;
    }

    device = 0;
    if (cmd == ATA_CMD_READ_DMA || cmd == ATA_CMD_READ_DMA_EXT ||
        cmd == ATA_CMD_WRITE_DMA || cmd == ATA_CMD_WRITE_DMA_EXT)
    {
        device |= 0x40;
    }

    port->cmd_table->cfis = (struct h2d_fis) {
        .type           = FIS_TYPE_H2D,
        .flags          = H2D_FIS_F_COMMAND,
        .command        = cmd,
        .lba_low        = lba & 0xff,
        .lba_mid        = (lba >> 8) & 0xff,
        .lba_high       = (lba >> 16) & 0xff,
        .lba_low_exp    = (lba >> 24) & 0xff,
        .lba_mid_exp    = (lba >> 32) & 0xff,
        .lba_high_exp   = (lba >> 40) & 0xff,
        .device         = device,
        .sector_count   = bytes / disk->storage.block_size,
    };
    port->cmd_table->prdt[0] = (struct ahci_prd) {
        .dba        = buf->paddr.items[0].start,
        .dbc        = bytes - 1,
    };

    if (acmd != NULL) {
        memcpy(port->cmd_table->acmd, acmd, 16);
    }

    flags = CMD_HEADER_F_FIS_LENGTH_5_DW;
    if (cmd == ATA_CMD_WRITE_DMA || cmd == ATA_CMD_WRITE_DMA_EXT) {
        flags |= CMD_HEADER_F_WRITE;
    }
    if (cmd == ATA_CMD_PACKET) {
        flags |= CMD_HEADER_F_ATAPI;
    }

    port->cmd_list[0] = (struct cmd_header) {
        .flags      = flags,
        .prdtl      = 1,
        .prdbc      = buf->size,
        .ctba0      = port->cmd_table_phys,
    };

    cdi_reset_wait_irq(disk->ahci->irq);
    pxreg_outl(disk->ahci, disk->port, REG_PxCI, 0x1);

    do {
        cdi_wait_irq(disk->ahci->irq, AHCI_IRQ_TIMEOUT);
        if (ahci_request_handle_error(disk, port->last_is) < 0) {
            return -1;
        }
        cdi_reset_wait_irq(disk->ahci->irq);
    } while (pxreg_inl(disk->ahci, disk->port, REG_PxCI) & 0x1);

    if (ahci_request_handle_error(disk, port->last_is) < 0) {
        return -1;
    }

    return 0;
}

static int ahci_identify(struct ahci_disk* disk)
{
    struct cdi_mem_area* buf;
    uint16_t* words;
    int ret;

    buf = cdi_mem_alloc(512, CDI_MEM_PHYS_CONTIGUOUS | CDI_MEM_DMA_4G);
    if (buf == NULL) {
        return -1;
    }

    ret = ahci_request(disk, ATA_CMD_IDENTIFY_DEVICE, 0, 512, buf, NULL);
    if (ret == 0) {
        words = buf->vaddr;
        disk->lba48 = !!(words[86] & (1 << 10));

        if (disk->lba48) {
            disk->storage.block_count = *(uint64_t*) &words[100];
        } else {
            disk->storage.block_count = *(uint32_t*) &words[60];
        }
    }

    cdi_mem_free(buf);

    return ret;
}

static int ahci_rw_blocks(struct cdi_storage_device* device, uint64_t start,
                          uint64_t count, void* buffer, bool read)
{
    struct ahci_disk* disk = (struct ahci_disk*) device;
    uint64_t bs = disk->storage.block_size;
    struct cdi_mem_area* buf;
    int cmd;
    int ret;

    buf = cdi_mem_alloc(count * bs,
                        CDI_MEM_PHYS_CONTIGUOUS | CDI_MEM_DMA_4G | 1);
    if (buf == NULL) {
        return -1;
    }

    if (read) {
        cmd = disk->lba48 ? ATA_CMD_READ_DMA_EXT : ATA_CMD_READ_DMA;
    } else {
        cmd = disk->lba48 ? ATA_CMD_WRITE_DMA_EXT : ATA_CMD_WRITE_DMA;
        memcpy(buf->vaddr, buffer, bs * count);
    }

    ret = ahci_request(disk, cmd, start, bs * count, buf, NULL);

    if (ret == 0 && read) {
        memcpy(buffer, buf->vaddr, bs * count);
    }

    cdi_mem_free(buf);
    return ret;
}

static int ahci_read_blocks(struct cdi_storage_device* device, uint64_t start,
                            uint64_t count, void* buffer)
{
    return ahci_rw_blocks(device, start, count, buffer, true);
}

static int ahci_write_blocks(struct cdi_storage_device* device, uint64_t start,
                             uint64_t count, void* buffer)
{
    return ahci_rw_blocks(device, start, count, buffer, false);
}


/* AHCI 1.3: "10.1.2 System Software Specific Initialization" */
/* See ahci_init_hardware() for first part of the initialisation */
static int ahci_disk_init(struct ahci_disk* disk)
{
    struct ahci_device* ahci = disk->ahci;
    int port = disk->port;
    struct ahci_port* p = &ahci->port[port];
    uint32_t cmd;

    /* The FIS must be 256-byte aligned */
    p->fis = cdi_mem_alloc(FIS_BYTES,
                           CDI_MEM_PHYS_CONTIGUOUS | CDI_MEM_DMA_4G | 8);
    if (p->fis == NULL) {
        printf("ahci: Could not allocate FIS\n");
        goto fail;
    }

    memset(p->fis->vaddr, 0, FIS_BYTES);

    p->fis_phys = p->fis->paddr.items[0].start;
    pxreg_outl(ahci, port, REG_PxFB, p->fis_phys);
    pxreg_outl(ahci, port, REG_PxFBU, 0); /* TODO Support 64 bit */

    /* The Command List must be 1k aligned */
    p->cmd_list_mem =
        cdi_mem_alloc(CMD_LIST_BYTES,
                      CDI_MEM_PHYS_CONTIGUOUS | CDI_MEM_DMA_4G | 10);
    if (p->cmd_list_mem == NULL) {
        printf("ahci: Could not allocate Command List\n");
        goto fail;
    }

    p->cmd_list = p->cmd_list_mem->vaddr;
    p->cmd_list_phys = p->cmd_list_mem->paddr.items[0].start;

    memset(p->cmd_list, 0, CMD_LIST_BYTES);

    pxreg_outl(ahci, port, REG_PxCLB, p->cmd_list_phys);
    pxreg_outl(ahci, port, REG_PxCLBU, 0); /* TODO Support 64 bit */

    /* Command tables must be 128-byte aligned */
    p->cmd_table_mem =
        cdi_mem_alloc(CMD_TABLE_BYTES,
                      CDI_MEM_PHYS_CONTIGUOUS | CDI_MEM_DMA_4G | 7);
    if (p->cmd_table_mem == NULL) {
        printf("ahci: Could not allocate Command Table\n");
        goto fail;
    }

    p->cmd_table = p->cmd_table_mem->vaddr;
    p->cmd_table_phys = p->cmd_table_mem->paddr.items[0].start;

    /* Enable FIS Receive and start processing command list */
    cmd = pxreg_inl(ahci, port, REG_PxCMD);
    pxreg_outl(ahci, port, REG_PxCMD, cmd | PxCMD_FRE | PxCMD_ST);

    /* Clear Serial ATA Error register*/
    pxreg_outl(ahci, port, REG_PxSERR, 0xffffffff);

    /* Set PxIE for interrupts */
    pxreg_outl(ahci, port, REG_PxIE, 0xffffffff);

    return 0;

fail:
    if (p->fis) {
        cdi_mem_free(p->fis);
        p->fis = NULL;
    }
    if (p->cmd_list) {
        cdi_mem_free(p->cmd_list_mem);
        p->cmd_list_mem = NULL;
    }
    if (p->cmd_table_mem) {
        cdi_mem_free(p->cmd_table_mem);
        p->cmd_table_mem = NULL;
    }
    return -1;
}

static struct cdi_device* disk_init_device(struct cdi_bus_data* bus_data)
{
    struct ahci_bus_data* ahci_bus_data = (struct ahci_bus_data*) bus_data;
    struct ahci_disk* disk;

    if (bus_data->bus_type != CDI_AHCI) {
        return NULL;
    }

    disk = calloc(1, sizeof(*disk));
    disk->ahci = ahci_bus_data->ahci;
    disk->port = ahci_bus_data->port;
    disk->storage.block_size = 512;
    disk->storage.dev.driver = &ahci_disk_driver.drv;
    asprintf((char**) &disk->storage.dev.name, "ahci%d", disk->port);

    if (ahci_disk_init(disk) < 0) {
        goto fail;
    }

    if (ahci_identify(disk) < 0) {
        goto fail;
    }

    cdi_storage_device_init(&disk->storage);

    return &disk->storage.dev;

fail:
    free(disk);
    return NULL;
}

static void disk_remove_device(struct cdi_device* device)
{
    /* TODO */
}

/**
 * Initialises the AHCI disk driver
 */
static int ahci_disk_driver_init(void)
{
    cdi_storage_driver_init(&ahci_disk_driver);
    return 0;
}

/**
 * Deinitialises the AHCI disk driver
 */
static int ahci_disk_driver_destroy(void)
{
    cdi_storage_driver_destroy(&ahci_disk_driver);

    /* TODO Deinitialise all devices */

    return 0;
}

static struct cdi_device* atapi_init_device(struct cdi_bus_data* bus_data)
{
    struct ahci_bus_data* ahci_bus_data = (struct ahci_bus_data*) bus_data;
    struct ahci_atapi* atapi;
    struct ahci_disk* disk;

    if (bus_data->bus_type != CDI_AHCI) {
        return NULL;
    }

    atapi = calloc(1, sizeof(*atapi));
    disk = &atapi->disk;
    disk->ahci = ahci_bus_data->ahci;
    disk->port = ahci_bus_data->port;
    disk->storage.block_size = 2048;

    if (ahci_disk_init(disk) < 0) {
        goto fail;
    }

    atapi->scsi.type = CDI_STORAGE;
    atapi->scsi.dev.driver = &ahci_atapi_driver.drv;
    asprintf((char**) &atapi->scsi.dev.name, "atapi%d", disk->port);

    cdi_scsi_device_init(&atapi->scsi);

    return &atapi->scsi.dev;

fail:
    free(atapi);
    return NULL;
}

static void atapi_remove_device(struct cdi_device* device)
{
    /* TODO */
}

/**
 * Initialises the AHCI ATAPI driver
 */
static int ahci_atapi_driver_init(void)
{
    cdi_scsi_driver_init(&ahci_atapi_driver);
    return 0;
}

/**
 * Deinitialises the AHCI ATAPI driver
 */
static int ahci_atapi_driver_destroy(void)
{
    cdi_scsi_driver_destroy(&ahci_atapi_driver);

    /* TODO Deinitialise all devices */

    return 0;
}

int ahci_scsi_request(struct cdi_scsi_device* scsi,
                      struct cdi_scsi_packet* packet)
{
    struct ahci_atapi* atapi = (struct ahci_atapi*) scsi;
    struct ahci_disk* disk = &atapi->disk;
    struct cdi_mem_area* buf;
    int ret;

    buf = cdi_mem_alloc(packet->bufsize,
                        CDI_MEM_PHYS_CONTIGUOUS | CDI_MEM_DMA_4G | 1);
    if (buf == NULL) {
        return -1;
    }

    memcpy(buf->vaddr, packet->buffer, packet->bufsize);
    ret = ahci_request(disk, ATA_CMD_PACKET, 0, packet->bufsize, buf,
                       packet->command);
    memcpy(packet->buffer, buf->vaddr, packet->bufsize);

    cdi_mem_free(buf);
    return ret;
}


struct cdi_storage_driver ahci_disk_driver = {
    .drv = {
        .type           = CDI_STORAGE,
        .bus            = CDI_AHCI,
        .name           = DISK_DRIVER_NAME,
        .init           = ahci_disk_driver_init,
        .destroy        = ahci_disk_driver_destroy,
        .init_device    = disk_init_device,
        .remove_device  = disk_remove_device,
    },
    .read_blocks        = ahci_read_blocks,
    .write_blocks       = ahci_write_blocks,
};

struct cdi_scsi_driver ahci_atapi_driver = {
    .drv = {
        .type           = CDI_SCSI,
        .bus            = CDI_AHCI,
        .name           = ATAPI_DRIVER_NAME,
        .init           = ahci_atapi_driver_init,
        .destroy        = ahci_atapi_driver_destroy,
        .init_device    = atapi_init_device,
        .remove_device  = atapi_remove_device,
    },
    .request            = ahci_scsi_request,
};

CDI_DRIVER(DISK_DRIVER_NAME, ahci_disk_driver)
CDI_DRIVER(ATAPI_DRIVER_NAME, ahci_atapi_driver)
