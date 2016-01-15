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
#include "list.h"

typedef enum{
	BAR_UNDEFINED, BAR_INVALID, BAR_UNUSED, BAR_32, BAR_64LO, BAR_64HI, BAR_IO
}TYPE_t;

typedef struct{
		uint32_t Address;
		size_t Size;
		TYPE_t Type;
}pciBar_t;

typedef struct{
		uint8_t Bus, Slot, Function;
		uint16_t DeviceID, VendorID;
		uint8_t ClassCode, Subclass, RevisionID, ProgIF, HeaderType;
		uint8_t irq;
		pciBar_t BAR[6];
}pciDevice_t;

extern list_t pciDevices;

void pci_Init();
uint32_t pci_readConfig(uint8_t bus, uint8_t slot, uint8_t func, uint8_t reg, uint8_t length);
void pci_writeConfig(uint8_t bus, uint8_t slot, uint8_t func, uint8_t reg, uint8_t length, uint32_t Data);

#endif /* PCI_H_ */

#endif
