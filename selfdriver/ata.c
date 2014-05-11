/*
 * ata.c
 *
 *  Created on: 25.12.2012
 *      Author: pascal
 */

#include "ata.h"
#include "stdint.h"
#include "util.h"
#include "stdbool.h"

//Ports
#define CHANNEL1	0x1F0	//1. Kanal
#define CHANNEL2	0x170
#define CHANNEL3	0x1E8
#define CHANNEL4	0x168

//Offsets
#define DATA	0x0		//Übermittlung der Lese/Schreib-Words
#define ERROR	0x1		//Fehlercode nach der Asuführung eines Befehls, gilt nur wenn im Statusregister das ERR-Bit gesetzt ist
#define SECTOR	0x2		//Anzahl der zu lesenden/schreibenden Sektoren
#define LBA_LOW	0x3		//LBA Bit 0-7
#define LBA_MID	0x4		//LBA Bit 8-15
#define LBA_HI	0x5		//LBA Bit 16-23
#define DEVICE	0x6		//LBA Bit 24-27 und Master/Slave
#define COMMAND	0x7		//Beim Lesen: Status-Register; beim Schreiben: Hier wird der Befehl reingeschrieben

#define CONTROL	0x206	//Beim Lesen: Status-Register; beim Schreiben: Steuerregister

//Kommandos (für Kommandoregister)
#define LBA24_WRITE		0x20
#define LBA24_READ		0x30
#define LBA48_WRITE		0x24
#define LBA48_READ		0x34

//Befehle
//Für alle Geräte die nur schreiben können
#define DIAGNOSTICS
#define FLUSH_CASH
#define IDENTIFY_DEVICE	0xEC
#define READ_DMA
#define READ_MULTIPLE
#define READ_SECTORS
#define READ_VERIFY_SE
#define SET_FEATURES
#define SET_MUL_MODE
//Für Geräte, die auch schreiben können
#define WRITE_DMA
#define WRITE_SECTORS
//zusätzliche Befehle für alle PACKET-Geräte
#define PACKET
#define DEVICE_RESET
#define IDENTIFY_PACKET
#define NOP
//zusätzliche optionale Befehle für PACKET-Geräte
#define READ_LOG_EXT
#define WRITE_LOG_EXT
#define READ_LOG_DMA_EXT
#define WRITE_LOG_DMA_EXT
//Verbotene Befehle für PACKET-Geräte
/*
 * DOWNLOAD_MICROCODE
 * READ_BUFFER
 * READ_DMA
 * READ_MULTIPLE
 * READ_VERIFY
 * SET_MULTIPLE_MODE
 * WRITE_BUFFER
 * WRITE_DMA
 * WRITE_MULTIPLE
 * WRITE_SECTORS
 * WRITE_UNCORRECTABLE
 */

#define MASTER			0xE0
#define SLAVE			0xF0

typedef struct{
		uint16_t Zero;
		uint16_t Obsolet;
		uint16_t SpecificConfig;
		uint16_t Obsolet2;
		uint16_t Retired;
} __attribute__((packed)) IdentifyData_t;

void send_Command(uint8_t);
bool check4Devices(uint16_t Channel);
bool ResponsiveDevice(uint16_t);

void ata_Init()
{
	if(check4Devices(CHANNEL1))
		printf("    Geraete auf Kanal 1 gefunden\n");
	if(check4Devices(CHANNEL2))
		printf("    Geraete auf Kanal 2 gefunden\n");
	if(check4Devices(CHANNEL3))
		printf("    Geraete auf Kanal 3 gefunden\n");
	if(check4Devices(CHANNEL4))
		printf("    Geraete auf Kanal 4 gefunden\n");
}

void send_Command(uint8_t command)
{
}

bool check4Devices(uint16_t Channel)
{
	bool Master, Slave;

	//HOB-Bit löschen
	outb(Channel + CONTROL, 0);

	//Statusregister auslesen (0xFF ist ein ungültiger Wert)
	outb(Channel + DEVICE, MASTER);
	if(inb(Channel + COMMAND) == 0xFF) return false;

	//Prüfen, ob ein Gerät "antwortet"
	if(!ResponsiveDevice(Channel)) return false;

	//IDENTIFY-DEVICE senden
	uint8_t *Data = malloc(512);

	return true;
}

bool ResponsiveDevice(uint16_t Channel)
{

	//Slave auswählen, da der Master auch antworten muss, wenn nur der Slave angesprochen wird
	//in der Realität ist es aber nicht so, deshalb wird weiter unten noch der Master angesprochen
	//irgendwelche Werte in ein LBA-Register schreiben
	outb(Channel + DEVICE, SLAVE);
	outb(Channel + LBA_LOW, 0xE7);
	outb(Channel + DEVICE, SLAVE);
	outb(Channel + LBA_MID, 0xF8);
	outb(Channel + DEVICE, SLAVE);
	outb(Channel + LBA_HI, 0xA3);

	//Sind die Werte jetzt gleich, ist mindestens ein Gerät vorhanden
	outb(Channel + DEVICE, SLAVE);
	if(inb(Channel + LBA_LOW) == 0xE7 && inb(Channel + LBA_MID) == 0xF8 && inb(Channel + LBA_HI) == 0xA3)
		return true;

	//Master auswählen (Ehrenrunde zur Sicherheit)
	//irgendwelche Werte in ein LBA-Register schreiben und wieder zurücklesen
	outb(Channel + DEVICE, MASTER);
	outb(Channel + LBA_LOW, 0xE7);
	outb(Channel + DEVICE, MASTER);
	outb(Channel + LBA_MID, 0xF8);
	outb(Channel + DEVICE, MASTER);
	outb(Channel + LBA_HI, 0xA3);

	//Sind die Werte jetzt gleich, ist mindestens ein Gerät vorhanden
	outb(Channel + DEVICE, MASTER);
	if(inb(Channel + LBA_LOW) == 0xE7 && inb(Channel + LBA_MID) == 0xF8 && inb(Channel + LBA_HI) == 0xA3)
		return true;

	return false;
}
