/*
 * pci.c
 *
 *  Created on: 10.03.2013
 *      Author: pascal
 */

#ifdef BUILD_KERNEL

#include "pci.h"
#include "config.h"
#include "util.h"
#include "stdbool.h"
#include "display.h"
#include "stdio.h"
#include "stdlib.h"

#define CONFIG_ADDRESS	0xCF8
#define CONFIG_DATA		0xCFC

#define PCI_VENDOR_ID   0x00 // length: 0x02 reg: 0x00 offset: 0x00
#define PCI_DEVICE_ID   0x02 // length: 0x02 reg: 0x00 offset: 0x02
#define PCI_COMMAND     0x04
#define PCI_STATUS      0x06
#define PCI_REVISION    0x08
#define PCI_CLASS       0x0B
#define PCI_SUBCLASS    0x0A
#define PCI_PROG_IF   	0x09
#define PCI_HEADERTYPE  0x0E
#define PCI_BAR0        0x10
#define PCI_BAR1        0x14
#define PCI_BAR2        0x18
#define PCI_BAR3        0x1C
#define PCI_BAR4        0x20
#define PCI_BAR5        0x24
#define PCI_CAPLIST     0x34
#define PCI_IRQLINE     0x3C

#define PCIBUSES      256
#define PCISLOTS    32
#define PCIFUNCS      8

//Aufbau des Kommandoregisters
/*
 * Bits	|	15-11	|		10			|			9				|		8		|	7		|			6			|
 * 		| Reserved	| Interupt Disable	| Fast Back-to-Back Enable	| SERR# Enable	| Reserved	| Parity Error Response	|
 *
 * Bits	|		5			|					4					|		3			|		2		|		1		|	0
 * 		| VGA Palette Snoop	| Memory Write and Invalidate Enable	| Special Cycles	| Bus Master	| Memory Space	| I/O Space
 */

//Aufbau des Statusregisters
/*
 * Bits	|			15			|			14			|			13			|			12			|			11			|
 * 		| Detected Parity Error	| Signaled System Error	| Received Master Abort	| Received Target Abort	| Signaled Target Abort	|
 *
 * Bits	|	10 + 9		|			8				|			7				|	6		|		5			|		4
 * 		| DEVSEL Timing	| Master Data Parity Error	| Fast Back-to-Back Capable	| Reserved	| 66 MHz Capable	| Capabilities List
 *
 * Bits	|		3			|	2-0
 * 		| Interrupt Status	| Reserved
 */

list_t pciDevices;

static bool checkDevice(uint8_t bus, uint8_t slot, uint8_t func)
{
	//Wenn VendorID != 0xFFFF, dann ist ein Gerät vorhanden
	if(pci_readConfig(bus, slot, func, PCI_VENDOR_ID, 2) != 0xFFFF) return true;
	return false;
}

static pciDevice_t *initDevice(uint8_t bus, uint8_t slot, uint8_t func)
{
	pciDevice_t *pciDevice;

	//Wenn kein Gerät vorhanden, Initialierung abbrechen
	if(!checkDevice(bus, slot, func))
		return NULL;

	//Ansonsten neue Struktur anlegen
	pciDevice = malloc(sizeof(pciDevice_t));
	if(pciDevice == NULL)
		return NULL;

	//Daten auslesen
	pciDevice->Bus = bus;
	pciDevice->Slot = slot;
	pciDevice->Function = func;
	pciDevice->DeviceID = pci_readConfig(bus, slot, 0, PCI_DEVICE_ID, 2);
	pciDevice->VendorID = pci_readConfig(bus, slot, 0, PCI_VENDOR_ID, 2);
	pciDevice->ClassCode = pci_readConfig(bus, slot, 0, PCI_CLASS, 1);
	pciDevice->Subclass = pci_readConfig(bus, slot, 0, PCI_SUBCLASS, 1);
	pciDevice->ProgIF = pci_readConfig(bus, slot, 0, PCI_PROG_IF, 1);
	pciDevice->RevisionID = pci_readConfig(bus, slot, 0, PCI_REVISION, 1);
	pciDevice->HeaderType = pci_readConfig(bus, slot, 0, PCI_HEADERTYPE, 1);
	pciDevice->irq = pci_readConfig(bus, slot, 0, PCI_IRQLINE, 1);

	if((pciDevice->HeaderType & ~0x80) == 0x00 || (pciDevice->HeaderType & ~0x80) == 0x01)
	{
		uint8_t maxBars = 6 - (pciDevice->HeaderType * 4);
		uint8_t Bar;
		for(Bar = 0; Bar < maxBars; Bar++)
		{
			//Offset der aktuellen Bar
			uint32_t BarOffset = 0x10 + (Bar * 4);

			uint32_t Data = pci_readConfig(bus, slot, 0, BarOffset, 4);

			//prüfen, ob Speicher oder IO
			if((Data & 0x1) == 0)
			{
				//Speicherressource:
				switch((Data & 0x6) >> 1)
				{
					case 0x0:	//32-Bit BAR
						//Mit lauter 1en überschreiben
						pci_writeConfig(bus, slot, 0, BarOffset, 4, 0xFFFFFFF0);
						//und zurück lesen
						uint32_t tmp = pci_readConfig(bus, slot, 0, BarOffset, 4);
						//Daten zurückschreiben
						pci_writeConfig(bus, slot, 0, BarOffset, 4, Data);
						if(tmp == 0)	//Es muss immer mind. ein Bit beschreibbar sein
							pciDevice->BAR[Bar].Type = BAR_UNUSED;
						else
						{
							pciDevice->BAR[Bar].Type = BAR_32;
							pciDevice->BAR[Bar].Address = Data & 0xFFFFFFF0;
							pciDevice->BAR[Bar].Size = ~(tmp & 0xFFFFFFF0) + 1;
						}
					break;
					case 0x2:	//64-Bit BAR
						//prüfen, ob ein 64-Bit-BAR an der aktuellen Position überhaupt möglich ist
						if(Bar >= (maxBars - 1))
							pciDevice->BAR[Bar].Type = BAR_INVALID;
						else
						{
							//Werte zwischenspeichern
							uint32_t tmp_high = pci_readConfig(bus, slot, 0, BarOffset + 4, 4);
							//Mit lauter 1en überschreiben
							pci_writeConfig(bus, slot, 0, BarOffset, 4, 0xFFFFFFF0);
							pci_writeConfig(bus, slot, 0, BarOffset + 4, 4, 0xFFFFFFFF);

							uint64_t tmp = pci_readConfig(bus, slot, 0, BarOffset, 4) | ((uint64_t)pci_readConfig(bus, slot, 0, BarOffset + 4, 4) << 32);
							//Daten zurückschreiben
							pci_writeConfig(bus, slot, 0, BarOffset, 4, Data);
							pci_writeConfig(bus, slot, 0, BarOffset + 4, 4, tmp_high);
							if(tmp == 0)	//Es muss immer mindestens ein Bit beschreibbar sein
								pciDevice->BAR[Bar].Type = pciDevice->BAR[Bar + 1].Type = BAR_UNUSED;
							else
							{
								pciDevice->BAR[Bar].Type = BAR_64LO;
								pciDevice->BAR[Bar].Address = Data & 0xFFFFFFF0;
								Bar++;									//Bar erhöhen, da die nachfolgende die oberen 32 Bit enthält
								pciDevice->BAR[Bar].Type = BAR_64HI;
								pciDevice->BAR[Bar].Address = tmp_high;
								pciDevice->BAR[Bar].Size = ~(tmp & ~0xF) + 1;
							}
						}
					break;
					default:
						pciDevice->BAR[Bar].Type = BAR_INVALID;
				}
			}
			else
			{
				//IO
				//Mit lauter 1en überschreiben
				pci_writeConfig(bus, slot, 0, BarOffset, 4, 0xFFFFFFFC);
				//und wieder zurücklesen
				uint32_t tmp = pci_readConfig(bus, slot, 0, BarOffset, 4);
				//Anfangszustand wiederherstellen
				pci_writeConfig(bus, slot, 0, BarOffset, 4, Data);
				if(tmp == 0)	//Es muss immer mind. ein Bit gesetzt bzw. beschreibbar sein
					pciDevice->BAR[Bar].Type = BAR_UNUSED;
				else
				{
					pciDevice->BAR[Bar].Type = BAR_IO;
					pciDevice->BAR[Bar].Address = Data & 0xFFFFFFFC;
					pciDevice->BAR[Bar].Size = ~(tmp & 0xFFFFFFFC) + 1;
				}
			}
		}
		//Die restlichen BARs als nicht definiert markieren
		uint8_t i;
		if(pciDevice->HeaderType == 0x1)
			for(i = 2; i < 6; i++)
				pciDevice->BAR[Bar].Type = BAR_UNDEFINED;
		if(pciDevice->HeaderType == 0x2)
			for(i = 0; i < 6; i++)
				pciDevice->BAR[Bar].Type = BAR_UNDEFINED;
	}

	return pciDevice;
}

static void checkSlot(uint8_t bus, uint8_t slot)
{
	pciDevice_t *pciDevice = initDevice(bus, slot, 0);
	if(pciDevice != NULL)
	{
		list_push(pciDevices, pciDevice);
		if(pciDevice->HeaderType & 0x80)	//Mehrere Funktionen werden unterstützt
		{
			//Überprüfe jede Funktion
			uint8_t func;
			for(func = 1; func < PCIFUNCS; func++)
			{
				pciDevice = initDevice(bus, slot, func);
				if(pciDevice != NULL)
					list_push(pciDevices, pciDevice);
			}
		}
	}
}

//#include "console.h"

void pci_Init()
{
	pciDevices = list_create();
	printf(" =>PCI:\n");
	uint16_t bus;
	uint8_t slot;
	for(bus = 0; bus < PCIBUSES; bus++)
		for(slot = 0; slot < PCISLOTS; slot++)
			checkSlot(bus, slot);

	SysLog("PCI", "Initialisierung abgeschlossen");
	printf("%u Geraete gefunden\n", list_size(pciDevices));
//	console_getch(&initConsole);
}

uint32_t pci_readConfig(uint8_t bus, uint8_t slot, uint8_t func, uint8_t reg, uint8_t length)
{
	uint32_t Data = 0;
	//Adressse generieren und schreiben
	outd(CONFIG_ADDRESS,
			(bus << 16)
			| (slot << 11)
			| (func << 8)
			| (reg & 0xFC)
			| 0x80000000);

	//Daten einlesen
	Data = ind(CONFIG_DATA) >> ((reg & ~0xFC) * 8);
	switch(length)
	{
		case 1:
			Data &= 0xFF;
		break;
		case 2:
			Data &= 0xFFFF;
		break;
		case 4:
			Data &= 0xFFFFFFFF;
		break;
	}
	return Data;
}

void pci_writeConfig(uint8_t bus, uint8_t slot, uint8_t func, uint8_t reg, uint8_t length, uint32_t Data)
{
	//Adressse generieren und schreiben
	outd(CONFIG_ADDRESS,
			(bus << 16)
			| (slot << 11)
			| (func << 8)
			| (reg & 0xFC)
			| 0x80000000);

	//Daten schreiben
	switch(length)
	{
		case 1:
			outb(CONFIG_DATA + (reg & 0x3), Data);
		break;
		case 2:
			outw(CONFIG_DATA + (reg & 0x2), Data);
		break;
		case 4:
			outd(CONFIG_DATA, Data);
		break;
	}
}

#endif
