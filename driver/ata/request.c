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
#include <string.h>

#include "cdi.h"
#include "cdi/storage.h"
#include "cdi/misc.h"
#include "cdi/io.h"

#include "device.h"

/**
 * Warten bis ein ATA-Geraet bereit ist
 *
 * @param bits Bits die gesetzt sein muessen
 * @param timeout Timeout in Millisekunden
 *
 * @return 1 wenn das Geraet bereit ist, 0 sonst
 */
static inline int ata_drv_wait_ready(struct ata_device* dev, uint8_t bits,
    uint32_t timeout)
{
    struct ata_controller* ctrl = dev->controller;
    uint32_t time = 0;

    ATA_DELAY(dev->controller);

    // Zuerst warten, bis das Busy-Bit nicht meht gesetzt ist, denn erst dann
    // sind die anderen Bits gueltig
    while (((ata_reg_inb(ctrl, REG_STATUS) & STATUS_BSY)) &&
        (time < timeout))
    {
        time += 10;
        cdi_sleep_ms(10);
    }

    // Dem Geraet etwas Zeit geben
    ATA_DELAY(dev->controller);

    // Jetzt koennen wir warten bis die gewuenschten Bits gesetzt sind
    while (((ata_reg_inb(ctrl, REG_STATUS) & bits) != bits) &&
        (time < timeout)) 
    {
        time += 10;
        cdi_sleep_ms(10);
    }

    return (time < timeout);
}


/**
 * Befehlsausfuehrung von einem ATA-Request starten
 *
 * @return 1 wenn der Befehl erfolgreich gestartet wurde, 0 sonst
 */
static int ata_request_command(struct ata_request* request)
{
    struct ata_device* dev = request->dev;
    struct ata_controller* ctrl = dev->controller;
    uint8_t control;
    
    // IRQ-Zaehler zuruecksetzen, egal ob er gebraucht wird oder nicht, stoert
    // ja niemanden
    cdi_reset_wait_irq(ctrl->irq);

    ata_drv_select(dev);

    // Warten bis das Geraet bereit ist
    if (!ata_drv_wait_ready(dev, 0, ATA_READY_TIMEOUT)) {
        request->error = DEVICE_READY_TIMEOUT;
        return 0;
    }
    
    // Device Register schreiben
    // TODO: nicht lba?
    ata_reg_outb(ctrl, REG_DEVICE, (request->flags.lba << 6) | (request->dev->
        id << 4) | ((request->registers.ata.lba >> 24) & 0xF));

    // Control Register schreiben
    control = 0;
    if (request->flags.poll) {
        // Wenn nur gepollt werden soll, muessen Interrupts deaktiviert werden
        control |= CONTROL_NIEN;
    }
    // TODO: HOB
    ata_reg_outb(ctrl, REG_CONTROL, control);

    // Features-Register schreiben
    ata_reg_outb(ctrl, REG_FEATURES, request->registers.ata.features);

    // Count-Register schrieben
    ata_reg_outb(ctrl, REG_SEC_CNT, request->registers.ata.count);
    
    // LBA Register schreiben
    ata_reg_outb(ctrl, REG_LBA_LOW, request->registers.ata.lba & 0xFF);
    ata_reg_outb(ctrl, REG_LBA_MID, (request->registers.ata.lba >> 8) & 0xFF);
    ata_reg_outb(ctrl, REG_LBA_HIG, (request->registers.ata.lba >> 16) &
        0xFF);

    // Befehl ausfuehren
    ata_reg_outb(ctrl, REG_COMMAND, request->registers.ata.command);

    return 1;
}

/**
 * Verarbeitet einen ATA-Request bei dem keine Daten uebertragen werden
 */
static int ata_protocol_non_data(struct ata_request* request)
{
    struct ata_device* dev = request->dev;
    struct ata_controller* ctrl = dev->controller;
    
    // Aktueller Status im Protokol
    enum {
        IRQ_WAIT,
        CHECK_STATUS
    } state;
    
    // Der Anfangsstatus haengt davon ab, ob gepollt werden soll oder nicht.
    if  (request->flags.poll) {
        state = CHECK_STATUS;
    } else {
        state = IRQ_WAIT;
    }

    while (1) {
        switch (state) {
            case IRQ_WAIT:
                // Auf IRQ warten
                if (!ata_wait_irq(ctrl, ATA_IRQ_TIMEOUT)) {
                    request->error = IRQ_TIMEOUT;
                    DEBUG("non_data IRQ-Timeout\n");
                    return 0;
                }

                // Jetzt muss der Status ueberprueft werden
                state = CHECK_STATUS;
                break;
            
            case CHECK_STATUS: {
                uint8_t status = ata_reg_inb(ctrl, REG_STATUS);
                
                // Status ueberpruefen
                if ((status & STATUS_BSY) == STATUS_BSY) {
                    // Wenn das Busy-Flag gesetzt ist, muss gewartet werden,
                    // bis es geloescht wird.
                    cdi_sleep_ms(20);
                } else {
                    return 1;
                }
                break;
            }
        }
    }
}

/**
 * Verarbeitet einen ATA-Request bei dem Daten ueber PIO eingelesen werden
 * sollen.
 * Nach Kapitel 11 in der ATA7-Spec
 */
int ata_protocol_pio_in(struct ata_request* request)
{
    struct ata_device* dev = request->dev;
    struct ata_controller* ctrl = dev->controller;
    size_t packet_size = 0;
    
    // Aktueller Status im Protokol
    enum {
        IRQ_WAIT,
        CHECK_STATUS,
        TRANSFER_DATA
    } state;
    
    // Der Anfangsstatus haengt davon ab, ob gepollt werden soll oder nicht.
    if  (request->flags.poll) {
        state = CHECK_STATUS;
    } else {
        state = IRQ_WAIT;
    }

    while (1) {
        switch (state) {
            case IRQ_WAIT:
                // Auf IRQ warten
                if (!ata_wait_irq(ctrl, ATA_IRQ_TIMEOUT)) {
                    request->error = IRQ_TIMEOUT;
                    DEBUG("pio_in IRQ-Timeout\n");
                    return 0;
                }

                if (request->flags.ata && packet_size==0) // ATAPI
                {
                    // Paketgroesse einlesen, da sonst unendlich viel gelesen wird
                    packet_size = ata_reg_inb(ctrl,REG_LBA_MID)|(ata_reg_inb(ctrl,REG_LBA_HIG)<<8);
                }

                // Jetzt muss der Status ueberprueft werden
                state = CHECK_STATUS;
                break;
            
            case CHECK_STATUS: {
                uint8_t status = ata_reg_inb(ctrl, REG_STATUS);
                
                // Status ueberpruefen
                // Wenn DRQ und BSY geloescht wurden ist irgendetwas schief
                // gelaufen.
                if ((status & (STATUS_BSY | STATUS_DRQ)) == 0) {
                    // TODO: Fehlerbehandlung
                    DEBUG("pio_in unerwarteter Status: 0x%x\n", status);
                    return 0;
                } else if ((status & STATUS_BSY) == STATUS_BSY) {
                    // Wenn das Busy-Flag gesetzt ist, muss gewartet werden,
                    // bis es geloescht wird.
                    ATA_DELAY(ctrl);
                } else if ((status & (STATUS_BSY | STATUS_DRQ)) == STATUS_DRQ)
                {
                    // Wenn nur DRQ gesetzt ist, sind Daten bereit um abgeholt
                    // zu werden. Transaktion nach Transfer Data
                    state = TRANSFER_DATA;
                }
                break;
            }

            case TRANSFER_DATA: {
                uint16_t* buffer = (uint16_t*) (request->buffer + (request->
                    blocks_done * request->block_size));

                // Wenn wir nicht pollen, muessen wir den Zahler zuruecksetzen,
                // da wir sonst moeglicherweise einen IRQ verpassen wenn wir das
                // erst nach dem insw machen. Das sollte auch nicht gleich nach
                // dem Warten gemacht werden, da es moeglich ist, dass der IRQ
                // nochmal feuert, wenn das Statusregister noch nicht gelesen
                // wurde.
                if (!request->flags.poll) {
                    cdi_reset_wait_irq(ctrl->irq);
                }

                ata_insw(ata_reg_base(ctrl, REG_DATA) + REG_DATA, buffer,
                    request->block_size / 2);

                // Anzahl der gelesenen Block erhoehen
                request->blocks_done++;
                
                // Naechste Transaktion ausfindig machen
                if (!request->flags.ata &&
                    request->blocks_done >= request->block_count) {
                    // Wenn alle Blocks gelesen wurden ist der Transfer
                    // abgeschlossen.
                    return 1;
                } else if (request->flags.ata && packet_size &&
                         request->blocks_done*request->block_size>=packet_size)
                {
                    // Wenn alle Bytes des ATAPI-Paketes gelesen wurden
                    // ist der Transfer abgeschlossen
                    return 1;
                } else if (request->flags.poll) {
                    // Wenn gepollt wird, muss jetzt gewartet werden, bis der
                    // Status wieder stimmt um den naechsten Block zu lesen.
                    state = CHECK_STATUS;
                } else {
                    // Bei der Benutzung von Interrupts wird jetzt auf den
                    // naechsten Interrupt gewartet
                    state = IRQ_WAIT;
                }
            }
        }
    }
}

/**
 * Verarbeitet einen ATA-Request bei dem Daten ueber PIO geschrieben werden
 * sollen
 */
int ata_protocol_pio_out(struct ata_request* request)
{
    struct ata_device* dev = request->dev;
    struct ata_controller* ctrl = dev->controller;
    size_t packet_size = 0;
    
    // Aktueller Status im Protokol
    enum {
        IRQ_WAIT,
        CHECK_STATUS,
        TRANSFER_DATA
    } state;

    state = CHECK_STATUS;
    while (1) {
        switch (state) {
            case IRQ_WAIT:
                // Auf IRQ warten
                if (!ata_wait_irq(ctrl, ATA_IRQ_TIMEOUT)) {
                    request->error = IRQ_TIMEOUT;
                    DEBUG("pio_out IRQ-Timeout\n");
                    return 0;
                }

                if (request->flags.ata && packet_size==0) // ATAPI
                {
                    // Paketgroesse einlesen, da sonst unendlich viel geschrieben wird
                    packet_size = ata_reg_inb(ctrl,REG_LBA_MID)|(ata_reg_inb(ctrl,REG_LBA_HIG)<<8);
                    DEBUG("packet_size = %d\n",packet_size);
                }

                // Jetzt muss der Status ueberprueft werden
                state = CHECK_STATUS;
                break;
            
            case CHECK_STATUS: {
                uint8_t status = ata_reg_inb(ctrl, REG_STATUS);
                
                if (request->flags.ata &&
                    request->blocks_done * request->block_size>=packet_size)
                {
                    // Das Paket wurde vollstaendig gelesen. DRQ wird nicht
                    // gesetzt, deswegen muss so beendet werden.
                        return 1;
                    }
                else if (!request->flags.ata &&
                         request->blocks_done==request->block_count)
                {
                    // Der Buffer wurde komplett gelesen. Dies sollte bei
                    // ATAPI nicht passieren!
                    return 1;
                }
                else if ((status & (STATUS_BSY | STATUS_DRQ)) == 0)
                {
                    // TODO: Fehlerbehandlung
                    DEBUG("pio_out unerwarteter Status: 0x%x\n", status);
                    return 0;

                } else if ((status & STATUS_BSY) == STATUS_BSY) {
                    // Wenn das Busy-Flag gesetzt ist, muss gewartet werden,
                    // bis es geloescht wird.
                    ATA_DELAY(ctrl);
                } else if ((status & (STATUS_BSY | STATUS_DRQ)) == STATUS_DRQ)
                {
                    // Wenn nur DRQ gesetzt ist, ist der Kontroller bereit um
                    // Daten zu empfangen.
                    // Transaktion nach Transfer Data
                    state = TRANSFER_DATA;
                }
                break;
            }

            case TRANSFER_DATA: {
                uint16_t* buffer = (uint16_t*) (request->buffer + (request->
                    blocks_done * request->block_size));

                // Wenn wir nicht pollen, muessen wir den Zaehler zuruecksetzen,
                // da wir sonst moeglicherweise einen IRQ verpassen wenn wir das
                // erst nach dem outsw machen. Das sollte auch nicht gleich nach
                // dem Warten gemacht werden, da es moeglich ist, dass der IRQ
                // nochmal feuert, wenn das Statusregister noch nicht gelesen
                // wurde.
                if (!request->flags.poll) {
                    cdi_reset_wait_irq(ctrl->irq);
                }

                // Einen Block schreiben
                ata_outsw(ata_reg_base(ctrl, REG_DATA) + REG_DATA, buffer,
                    request->block_size / 2);

                // Anzahl der geschriebenen Block erhoehen
                request->blocks_done++;
                
                // Naechste Transaktion ausfindig machen
                if (request->flags.poll) {
                    // Wenn gepollt wird, muss jetzt gewartet werden, bis der
                    // Status wieder stimmt um den naechsten Block zu
                    // schreiben.
                    state = CHECK_STATUS;
                } else {
                    // Bei der Benutzung von Interrupts wird jetzt auf den
                    // naechsten Interrupt gewartet
                    state = IRQ_WAIT;
                }
            }
        }
    }
}

/**
 * Initialisiert DMA fuer einen Transfer
 */
static int ata_request_dma_init(struct ata_request* request)
{
    struct ata_device* dev = request->dev;
    struct ata_controller* ctrl = dev->controller;
    uint64_t size = request->block_size * request->block_count;

    *ctrl->prdt_virt = (uint32_t) ctrl->dma_buf_phys;
    // Groesse nicht ueber 64K, 0 == 64K
    *ctrl->prdt_virt |= (size & (ATA_DMA_MAXSIZE - 1)) << 32L;
    // Letzter Eintrag in PRDT
    *ctrl->prdt_virt |= (uint64_t) 1L << 63L;

    // Die laufenden Transfers anhalten
    cdi_outb(ctrl->port_bmr_base + BMR_COMMAND, 0);
    cdi_outb(ctrl->port_bmr_base + BMR_STATUS,
        cdi_inb(ctrl->port_bmr_base + BMR_STATUS) | BMR_STATUS_ERROR |
        BMR_STATUS_IRQ);

    // Adresse der PRDT eintragen
    cdi_outl(ctrl->port_bmr_base + BMR_PRDT, ctrl->prdt_phys);

    if (request->flags.direction != READ) {
        memcpy(ctrl->dma_buf_virt, request->buffer, size);
    }
    return 1;
}

/**
 * Verarbeitet einen ATA-Request bei dem Daten ueber DMA uebertragen werden
 * sollen
 */
static int ata_protocol_dma(struct ata_request* request)
{
    struct ata_device* dev = request->dev;
    struct ata_controller* ctrl = dev->controller;

    // Aktueller Status im Protokoll
    enum {
        IRQ_WAIT,
        CHECK_STATUS,
    } state;

    // Wozu das lesen und dieser Register gut ist, weiss ich nicht, doch ich
    // habe es so in verschiedenen Treibern gesehen, deshalb gehe ich mal davon
    // aus, dass es notwendig ist.
    cdi_inb(ctrl->port_bmr_base + BMR_COMMAND);
    cdi_inb(ctrl->port_bmr_base + BMR_STATUS);

    // Busmastering starten
    if (request->flags.direction == READ) {
        cdi_outb(ctrl->port_bmr_base + BMR_COMMAND,
            BMR_CMD_START | BMR_CMD_READ);
    } else {
        cdi_outb(ctrl->port_bmr_base + BMR_COMMAND, BMR_CMD_START);
    }
    cdi_inb(ctrl->port_bmr_base + BMR_COMMAND);
    cdi_inb(ctrl->port_bmr_base + BMR_STATUS);


    if (request->flags.poll) {
        state = CHECK_STATUS;
    } else {
        state = IRQ_WAIT;
    }

    while (1) {
        switch (state) {
            case IRQ_WAIT:
                // Auf IRQ warten
                if (!ata_wait_irq(ctrl, ATA_IRQ_TIMEOUT)) {
                    request->error = IRQ_TIMEOUT;
                    DEBUG("dma IRQ-Timeout\n");
                    return 0;
                }

                state = CHECK_STATUS;
                break;

            case CHECK_STATUS: {
                uint8_t status = ata_reg_inb(ctrl, REG_STATUS);

                // Sicherstellen dass der Transfer abgeschlossen ist. Bei
                // polling ist das hier noch nicht umbedingt der Fall.
                if ((status & (STATUS_BSY | STATUS_DRQ)) == 0) {
                    cdi_inb(ctrl->port_bmr_base + BMR_STATUS);
                    cdi_outb(ctrl->port_bmr_base + BMR_COMMAND, 0);
                    goto out_success;
                }
                break;
            }
        }
    }

out_success:
    if (request->flags.direction == READ) {
        memcpy(request->buffer, ctrl->dma_buf_virt,
            request->block_size * request->block_count);
    }
    return 1;
}

/**
 * Fuehrt einen ATA-Request aus.
 *
 * @return 1 Wenn der Request erfolgreich bearbeitet wurde, 0 sonst
 */
int ata_request(struct ata_request* request)
{
   // printf("ata: [%d:%d] Request command=%x count=%x lba=%llx protocol=%x\n", request->dev->controller->id, request->dev->id, request->registers.ata.command, request->registers.ata.count, request->registers.ata.lba, request->protocol);

    // Bei einem DMA-Request muss der DMA-Controller erst vorbereitet werden
    if ((request->protocol == DMA) && !ata_request_dma_init(request)) {
        DEBUG("ata: Fehler beim Initialisieren des DMA-Controllers\n");
        return 0;
    }

    // Befehl ausfuehren
    if (!ata_request_command(request)) {
        DEBUG("Fehler bei der Befehlsausfuehrung\n");
        return 0;
    }

    // Je nach Protokoll werden jetzt die Daten uebertragen
    switch (request->protocol) {
        case NON_DATA:
            if (!ata_protocol_non_data(request)) {
                return 0;
            }
            break;
        
        case PIO:
            if ((request->flags.direction == READ) &&
                (!ata_protocol_pio_in(request)))
            {
                return 0;
            } else if ((request->flags.direction == WRITE) &&
                (!ata_protocol_pio_out(request)))
            {
                return 0;
            }
            break;

        case DMA:
            if (!ata_protocol_dma(request)) {
                return 0;
            }
            break;

        default:
            return 0;
    }
    return 1;
}

