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

#include <stdio.h>
#include <stdlib.h>

#include "cdi.h"
#include "cdi/storage.h"
#include "cdi/misc.h"
#include "cdi/io.h"

#include "device.h"

/**
 * ATA-Geraet identifizieren
 *
 * @return 0 Wenn das Geraet erfolgreich identifiziert wurde, != 0 sonst
 */
int ata_drv_identify(struct ata_device* dev)
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
        .registers.ata.command = IDENTIFY_DEVICE,
        .block_count = 1,
        .block_size = ATA_SECTOR_SIZE,
        .buffer = &id,

        .error = 0
    };
    
    // Request starten
    if (!ata_request(&request)) {
        // Wenn ein Fehler aufgetreten ist, koennen wir es noch mit einem
        // IDENTIFY PACKET DEVICE probieren.
        return atapi_drv_identify(dev);
    }
    
    // Pruefen, welche LBA-Modi dabei sind
    if (id.features_support.bits.lba48) {
        dev->lba48 = 1;
        dev->dev.storage.block_count = id.max_lba48_address;
    }
    if (id.capabilities.lba) {
        dev->lba28 = 1;
        dev->dev.storage.block_count = id.lba_sector_count;
    }

    // Pruefen ob DMA unterstuetzt wird
    if (id.capabilities.dma) {
        dev->dma = 1;
    }

    // Wenn keiner der LBA-Modi unterstuetzt wird, muss abgebrochen werden, da
    // CHS noch nicht implementiert ist.
    if (!dev->lba48 && !dev->lba28) {
        DEBUG("Geraet unterstuetzt nur CHS.\n");
        return 0;
    }

    // Ein ATA-Geraet
    dev->atapi = 0;



    return 1;
}

/**
 * Sektoren von einem ATA-Geraet lesen oder darauf schreiben
 *
 * @param direction 0 fuer lesen, 1 fuer schreiben
 * @param start LBA des Startsektors
 * @param count Anzahl der Sektoren
 * @param buffer Pointer auf den Puffer in dem die Daten abgelegt werden
 * sollen, respektiv aus dem sie gelesen werden sollen.
 *
 * @return 1 wenn die Blocks erfolgreich gelesen/geschrieben wurden, 0 sonst
 */
static int ata_drv_rw_sectors(struct ata_device* dev, int direction,
    uint64_t start, size_t count, void* buffer)
{
    int result = 1;
    struct ata_request request;
    // Da nicht mehr als 256 Sektoren auf einmal gelesen/geschrieben werden
    // koennen, muss unter Umstaenden mehrmals gelesen/geschrieben werden.
    uint16_t current_count;
    void* current_buffer = buffer;
    uint64_t lba = start;
    int max_count;
    int again = 2;
    // Anzahl der Sektoren die noch uebrig sind
    size_t count_left = count;



    // Request vorbereiten
    request.dev = dev;
    request.flags.poll = 0;
    request.flags.ata = 0;
    request.flags.lba = 1;

    // Richtung festlegen
    if (direction == 0) {
        request.flags.direction = READ;
    } else {
        request.flags.direction = WRITE;
    }


    if (dev->dma && dev->controller->dma_use) {
        // DMA
        max_count = ATA_DMA_MAXSIZE / ATA_SECTOR_SIZE;
        if (direction) {
            request.registers.ata.command = WRITE_SECTORS_DMA;
        } else {
            request.registers.ata.command = READ_SECTORS_DMA;
        }
        request.protocol = DMA;
    } else {
        // PIO
        max_count = 256;
        if (direction) {
            request.registers.ata.command = WRITE_SECTORS;
        } else {
            request.registers.ata.command = READ_SECTORS;
        }
        request.protocol = PIO;

        // Mit PIO pollen wir lieber, da IRQs dort _wirklich langsam_ sind
        request.flags.poll = 1;
    }

    // Solange wie noch Sektoren uebrig sind, wird gelesen
    while (count_left > 0) {
        // Entscheiden wieviele Sektoren im aktuellen Durchlauf gelesen werden
        if (count_left > max_count) {
            current_count = max_count;
        } else {
            current_count = count_left;
        }
        

        // Achtung: Beim casten nach uint8_t wird bei 256 Sektoren eine 0.
        // Das macht aber nichts, da in der Spezifikation festgelegt ist,
        // dass 256 Sektoren gelesen werden sollen, wenn im count-Register
        // 0 steht.
        request.registers.ata.count = (uint8_t) current_count;
        request.registers.ata.lba = lba;

        request.block_count = current_count;
        request.block_size = ATA_SECTOR_SIZE;
        request.blocks_done = 0;
        request.buffer = current_buffer;

        request.error = NO_ERROR;
        
        // TODO: LBA48
        // TODO: CHS
        
        // Request ausfuehren
        if (!ata_request(&request)) {
            if (again) {
                again--;
                continue;
            }
            result = 0;
            break;
        }

        // Pufferpointer und Anzahl der uebrigen Blocks anpassen
        current_buffer += current_count * ATA_SECTOR_SIZE;
        count_left -= current_count;
        lba += current_count;
        again = 2;
    }

    return result;
}


/**
 * Sektoren von einem ATA-Geraet lesen
 *
 * @param start LBA des Startsektors
 * @param count Anzahl der Sektoren
 * @param buffer Pointer auf den Puffer in dem die Daten abgelegt werden sollen
 *
 * @return 1 wenn die Blocks erfolgreich gelesen wurden, 0 sonst
 */
int ata_drv_read_sectors(struct ata_device* dev, uint64_t start, size_t count,
    void* buffer)
{
    return ata_drv_rw_sectors(dev, 0, start, count, buffer);
}

/**
 * Sektoren auf eine ATA-Geraet schreiben
 *
 * @param start LBA des Startsektors
 * @param count Anzahl der Sektoren
 * @param buffer Pointer auf den Puffer aus dem die Daten gelesen werden sollen
 *
 * @return 1 wenn die Blocks erfolgreich geschrieben wurden, 0 sonst
 */
int ata_drv_write_sectors(struct ata_device* dev, uint64_t start, size_t count,
    void* buffer)
{
    return ata_drv_rw_sectors(dev, 1, start, count, buffer);
}

