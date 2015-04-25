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

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "cdi/lists.h"
#include "cdi/pci.h"
#include "cdi/misc.h"
#include "cdi.h"

#include "ahci.h"

#define DRIVER_NAME "ahci"

static struct cdi_storage_driver ahci_driver;


/* AHCI 1.3: "10.1.2 System Software Specific Initialization" */
/* See ahci_disk_init() for second part of the initialisation */
static int ahci_init_hardware(struct ahci_device* ahci)
{
    int port;
    bool all_idle;
    uint32_t cmd, ssts, sig, sctl;
    int retries;

    /* HBA reset (see AHCI 1.3, chapter 10.4.3) */
    reg_outl(ahci, REG_GHC, GHC_AE);
    reg_outl(ahci, REG_GHC, GHC_AE | GHC_HR);

    while (reg_inl(ahci, REG_GHC) & GHC_HR) {
        /* wait for reset to complete */
    }

    reg_outl(ahci, REG_GHC, GHC_AE | GHC_IE);

    /* Determine implemented ports */
    ahci->ports = reg_inl(ahci, REG_PI);

    /* Ensure that all ports are idle */
    retries = 10;
    do {
        all_idle = true;
        for (port = 0; port < MAX_PORTS; port++) {
            if (!(ahci->ports & BIT(port))) {
                continue;
            }
            cmd = pxreg_inl(ahci, port, REG_PxCMD);
            if (cmd & (PxCMD_ST | PxCMD_CR | PxCMD_FR | PxCMD_FRE)) {
                all_idle = false;
                cmd &= ~(PxCMD_ST | PxCMD_FR);
                pxreg_outl(ahci, port, REG_PxCMD, cmd);
            }
        }
        if (!all_idle) {
            cdi_sleep_ms(100);
        }
    } while (!all_idle && --retries);

    if (!all_idle) {
        printf("ahci: Couldn't place ports into idle state\n");
    }

    /* Determine number of command slots */
    ahci->cmd_slots = (reg_inl(ahci, REG_CAP) & CAP_NCS_MASK) >> CAP_NCS_SHIFT;

    /* All ports: Power On Device, Spin-Up Device, Link Active */
    for (port = 0; port < MAX_PORTS; port++) {
        if (!(ahci->ports & BIT(port))) {
            continue;
        }
        cmd = pxreg_inl(ahci, port, REG_PxCMD);
        cmd &= ~PxCMD_ICC_MASK;
        cmd |=  PxCMD_POD | PxCMD_SUD | PxCMD_ICC_ACTIVE;
        pxreg_outl(ahci, port, REG_PxCMD, cmd);
    }

    /* Detect and initialise devices for each port */
    retries = 10;
    for (port = 0; port < MAX_PORTS; port++) {
        if (!(ahci->ports & BIT(port))) {
            continue;
        }

        /* If we know there is nothing (Cold Presence Detection supported, but
         * no device detected), skip the device. */
        cmd = pxreg_inl(ahci, port, REG_PxCMD);
        if ((cmd & (PxCMD_CPD | PxCMD_CPS)) == PxCMD_CPD) {
            continue;
        }

        /* Check whether there is a device */
        do {
            ssts = pxreg_inl(ahci, port, REG_PxSSTS);
            if ((ssts & PxSSTS_DET_MASK) == PxSSTS_DET_PRESENT) {
                break;
            } else if ((ssts & PxSSTS_DET_MASK) == 0 && retries < 10) {
                /* At least device presence should be detected after 100ms,
                 * even if Phy communication isn't established yet. */
                break;
            } else if (retries) {
                cdi_sleep_ms(100);
                retries--;
            }
        } while (retries);

        /* If there is a device attached, register it */
        if ((ssts & PxSSTS_DET_MASK) != PxSSTS_DET_PRESENT ||
            (ssts & PxSSTS_IPM_MASK) != PxSSTS_IPM_ACTIVE)
        {
            continue;
        }

        /* Forbid transition to any power mangement states */
        sctl = pxreg_inl(ahci, port, REG_PxSCTL);
        sctl &= ~PxSCTL_IPM_MASK;
        sctl |= PxSCTL_IPM_NONE;
        pxreg_outl(ahci, port, REG_PxSCTL, sctl);

        /* Create a device with the right driver */
        sig = pxreg_inl(ahci, port, REG_PxSIG);

        switch (sig) {
            case SATA_SIG_DISK: {
                struct ahci_bus_data bus_data = {
                    .cdi = {
                        .bus_type   = CDI_AHCI,
                    },
                    .ahci = ahci,
                    .port = port,
                };
                cdi_provide_device_internal_drv(&bus_data.cdi,
                                                &ahci_disk_driver.drv);
                break;
            }
            case SATA_SIG_QEMU_CD:
            case SATA_SIG_PACKET: {
                struct ahci_bus_data bus_data = {
                    .cdi = {
                        .bus_type   = CDI_AHCI,
                    },
                    .ahci = ahci,
                    .port = port,
                };
                cdi_provide_device_internal_drv(&bus_data.cdi,
                                                &ahci_atapi_driver.drv);
                break;
            }
            default:
                printf("ahci: Can't handle device with signature %x", sig);
                break;
        }
    }

    return 0;
}

void ahci_port_comreset(struct ahci_device* ahci, int port)
{
    uint32_t sctl = pxreg_inl(ahci, port, REG_PxSCTL);
    uint32_t cmd = pxreg_inl(ahci, port, REG_PxCMD);

    pxreg_outl(ahci, port, REG_PxCMD, cmd & ~PxCMD_ST);
    while (pxreg_inl(ahci, port, REG_PxCMD) & PxCMD_CR);

    sctl &= ~PxSCTL_DET_MASK;
    sctl |= 0x1;
    pxreg_outl(ahci, port, REG_PxSCTL, sctl);

    cdi_sleep_ms(1);

    sctl &= ~PxSCTL_DET_MASK;
    pxreg_outl(ahci, port, REG_PxSCTL, sctl);

    pxreg_outl(ahci, port, REG_PxCMD, cmd);
}

static void irq_handler(struct cdi_device* dev)
{
    struct ahci_device* ahci = (struct ahci_device*) dev;
    uint32_t is;
    int port;

    is = reg_inl(ahci, REG_IS);

    for (port = 0; port < MAX_PORTS; port++) {
        struct ahci_port* p = &ahci->port[port];
        uint32_t port_is;

        if (!(is & ahci->ports & BIT(port))) {
            continue;
        }

        port_is = pxreg_inl(ahci, port, REG_PxIS);
        pxreg_outl(ahci, port, REG_PxIS, port_is);
        p->last_is = port_is;
    }

    /* For the actual interrupt handling, we use cdi_wait_irq(). */
    reg_outl(ahci, REG_IS, is);
}

static struct cdi_device* ahci_init_device(struct cdi_bus_data* bus_data)
{
    struct cdi_pci_device* pci = (struct cdi_pci_device*) bus_data;
    struct ahci_device* ahci;
    int i, ret;

    /* Check PCI class/subclass */
    if (pci->class_id != PCI_CLASS_STORAGE ||
        pci->subclass_id != PCI_SUBCLASS_ST_SATA ||
        pci->interface_id != 1)
    {
        return NULL;
    }

    ahci = calloc(1, sizeof(*ahci));

    /* Map AHCI BAR */
    struct cdi_pci_resource* res;
    cdi_list_t reslist = pci->resources;

    for (i = 0; (res = cdi_list_get(reslist, i)); i++) {
        if (res->index == 5 && res->type == CDI_PCI_MEMORY) {
            ahci->mmio = cdi_mem_map(res->start, res->length);
        }
    }

    if (ahci->mmio == NULL) {
        printf("ahci: Could not find MMIO area\n");
        goto fail;
    }

    ahci->irq = pci->irq;
    cdi_register_irq(ahci->irq, irq_handler, &ahci->dev);

    /* The actual hardware initialisation as described in the spec */
    ret = ahci_init_hardware(ahci);
    if (ret < 0) {
        goto fail;
    }

    return &ahci->dev;

fail:
    free(ahci);
    return NULL;
}

static void ahci_remove_device(struct cdi_device* device)
{
    /* TODO */
}

/**
 * Initialises the AHCI controller driver
 */
static int ahci_driver_init(void)
{
    cdi_storage_driver_init(&ahci_driver);

    return 0;
}

/**
 * Deinitialises the AHCI controller driver
 */
static int ahci_driver_destroy(void)
{
    cdi_storage_driver_destroy(&ahci_driver);

    /* TODO Deinitialise all devices */

    return 0;
}

static struct cdi_storage_driver ahci_driver = {
    .drv = {
        .type           = CDI_AHCI,
        .bus            = CDI_PCI,
        .name           = DRIVER_NAME,
        .init           = ahci_driver_init,
        .destroy        = ahci_driver_destroy,
        .init_device    = ahci_init_device,
        .remove_device  = ahci_remove_device,
    },
};

CDI_DRIVER(DRIVER_NAME, ahci_driver)
