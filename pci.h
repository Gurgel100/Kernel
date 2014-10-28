/*
 * pci.h
 *
 *  Created on: 10.03.2013
 *      Author: pascal
 */

#ifdef BUILD_KERNEL

#ifndef PCI_H_
#define PCI_H_

#include "stdint.h"
#include "stddef.h"

typedef enum{
	BAR_UNDEFINED, BAR_INVALID, BAR_UNUSED, BAR_32, BAR_64LO, BAR_64HI, BAR_IO
}TYPE_t;

typedef struct{
		uint32_t Address;
		size_t Size;
		TYPE_t Type;
}pciBar_t;

typedef struct{
		uint8_t Bus, Slot;
		uint16_t DeviceID, VendorID;
		uint8_t ClassCode, Subclass, RevisionID, ProgIF, HeaderType;
		uint8_t Functions;	//Bit-Maske welche Funktionen zur Verfügung stehen
		uint8_t irq;
		pciBar_t BAR[6];
		void *NextDevice;
}pciDevice_t;

pciDevice_t *pci_firstDevice;
uint64_t NumDevices;

void pci_Init();

#endif /* PCI_H_ */

#endif
