/*
 * Copyright (c) 2007 The tyndur Project. All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>

#include "cdi.h"
#include "cdi/storage.h"
#include "cdi/misc.h"
#include "cdi/io.h"
#include "cdi/scsi.h"

#include "device.h"


/**
 * ATAPI-Geraet identifizieren
 *
 * @return 0 Wenn das Geraet erfolgreich identifiziert wurde, != 0 sonst
 */
int atapi_drv_identify(struct ata_device* dev)
{
    struct ata_identfiy_data id;

    // Request vorbereiten
    struct ata_request request = {
        .dev = dev,

        .flags.direction = READ,
        .flags.poll = 1,
        .flags.lba = 0,

        // Die Identifikationsdaten werden ueber PIO DATA IN gelesen
        .protocol = PIO,
        .registers.ata.command = IDENTIFY_PACKET_DEVICE,
        .block_count = 1,
        .block_size = ATA_SECTOR_SIZE,
        .buffer = &id,

        .error = 0
    };
    
    // Request starten
    if (!ata_request(&request)) {
        // Pech gehabt
        return 0;
    }
    
    // Es handelt sich um ein ATAPI-Geraet
    dev->atapi = 1;

    return 1;
}

void atapi_init_device(struct ata_device* device)
{
    struct cdi_scsi_device* scsi = (struct cdi_scsi_device*) device;

    scsi->type = CDI_STORAGE;
    cdi_scsi_device_init(scsi);
}

void atapi_remove_device(struct cdi_device* device)
{

}

int atapi_request(struct cdi_scsi_device* scsi,struct cdi_scsi_packet* packet)
{
    struct ata_device *dev = (struct ata_device*)scsi;
    struct ata_request request = {
        .dev = dev,
        .protocol = PIO,
        .flags = {
            .direction = WRITE,
            .poll = 1,
            .ata = 0, // Ich brauch nur ATA-Befehle (Packet)
            .lba = 0 // Der Kommentar in device.h ist kein deutscher Satz!
        },
        .registers = {
            .ata = {
                .command = PACKET,
                .lba = 0x00FFFF00 // Sonst hat bei mir nachher nichts in den
                                  // LBA-Registern dringestanden und die
                                  // braucht man ja fuer die Paketgroesse.
            }
        },
        .block_count = 1,
        .block_size = packet->cmdsize,
        .blocks_done = 0,
        .buffer = packet->command,
        .error = 0
    };

    if (ata_request(&request))
    {
        int status;
        struct ata_request rw_request = {
            .dev = dev,
            .flags = {
                .poll = 1,
                .lba = 1,
                .ata = 1
            },
            .block_count = 1,
            .block_size = packet->bufsize,
            .blocks_done = 0,
            .buffer = packet->buffer,
            .error = 0
        };

        // Lesen bzw. Schreiben der Daten
        // TODO: DMA
        if (packet->direction==CDI_SCSI_READ) ata_protocol_pio_in(&rw_request);
        else if (packet->direction==CDI_SCSI_WRITE) ata_protocol_pio_in(&rw_request);

        // Bei Fehler den Sense Key zurueckgeben
        status = ata_reg_inb(dev->controller, REG_STATUS);
        if (status & STATUS_ERR) {
            return (ata_reg_inb(dev->controller, REG_ERROR) >> 4);
        } else {
            return 0;
        }
    }

    return 0xB;
}
