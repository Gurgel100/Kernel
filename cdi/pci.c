/*
 * pci.c
 *
 *  Created on: 02.08.2013
 *      Author: pascal
 */

#include "pci.h"
#include "../pci.h"
#include "lists.h"

/**
 * \german
 * Gibt alle PCI-Geräte im System zurück. Die Geräte (struct cdi_pci_device*)
 * werden dazu in die übergebene Liste eingefügt.
 * \endgerman
 * \english
 * Queries all PCI devices in the machine. The devices (struct cdi_pci_device*)
 * are inserted into a given list.
 * \endenglish
 */
void cdi_pci_get_all_devices(cdi_list_t list)
{
	pciDevice_t *pciDevice;
	for(pciDevice = pci_firstDevice; pciDevice != NULL; pciDevice = pciDevice->NextDevice)
	{
		struct cdi_pci_device *cdi_pciDevice;
		cdi_list_t resources = cdi_list_create();

		if(pciDevice->HeaderType == 0x00 || pciDevice->HeaderType == 0x01)
		{
			uint8_t maxBars = 6 - (pciDevice->HeaderType * 4);
			uint8_t Bar;
			for(Bar = 0; Bar < maxBars; Bar++)
			{
				struct cdi_pci_resource *cdi_pciRes;
				//Falls die Bar nicht benutzt wird, überspringen
				if(pciDevice->BAR[Bar].Type == BAR_INVALID || pciDevice->BAR[Bar].Type == BAR_UNUSED
						|| pciDevice->BAR[Bar].Type == BAR_UNDEFINED)
					continue;
				cdi_pciRes = malloc(sizeof(*cdi_pciRes));
				switch(pciDevice->BAR[Bar].Type)
				{
					case BAR_32:	//32Bit BAR
						*cdi_pciRes = (struct cdi_pci_resource){
							.type = CDI_PCI_MEMORY,
							.start = pciDevice->BAR[Bar].Address,
							.length = pciDevice->BAR[Bar].Size,
							.index = Bar
						};
					break;
					case BAR_64LO:	//64Bit BAR
						if(pciDevice->BAR[Bar + 1].Type == BAR_64HI)
						{
							*cdi_pciRes = (struct cdi_pci_resource){
								.type = CDI_PCI_MEMORY,
								.start = pciDevice->BAR[Bar].Address | (pciDevice->BAR[Bar + 1].Address << 32),
								.length = pciDevice->BAR[Bar + 1].Size,
								.index = Bar
							};
							Bar++;	//Eine BAR überspringen (BAR_64_HI)
						}
					break;
					case BAR_IO:	//IO BAR
						*cdi_pciRes = (struct cdi_pci_resource){
							.type = CDI_PCI_IOPORTS,
							.start = pciDevice->BAR[Bar].Address,
							.length = pciDevice->BAR[Bar].Size,
							.index = Bar
						};
				}
				cdi_list_push(resources, cdi_pciRes);
			}
		}

		cdi_pciDevice = malloc(sizeof(*cdi_pciDevice));
		*cdi_pciDevice = (struct cdi_pci_device){
			.bus_data = CDI_PCI,
			.bus = pciDevice->Bus,
			.dev = pciDevice->Slot,
			.function = pciDevice->Functions,
			.vendor_id = pciDevice->VendorID,
			.device_id = pciDevice->DeviceID,
			.class_id = pciDevice->ClassCode,
			.subclass_id = pciDevice->Subclass,
			.interface_id = pciDevice->ProgIF,
			.rev_id = pciDevice->RevisionID,
			.irq = pciDevice->irq,
			.resources = resources
		};
		cdi_list_push(list, cdi_pciDevice);
	}
}

/**
 * \german
 * Gibt die Information zu einem PCI-Gerät frei
 * \endgerman
 * \english
 * Frees the information for a PCI device
 * \endenglish
 */
void cdi_pci_device_destroy(struct cdi_pci_device* device)
{
	free(device);
}

/**
 * \if german
 * Liest ein Word (16 Bit) aus dem PCI-Konfigurationsraum eines PCI-Geraets
 *
 * @param device Das Geraet
 * @param offset Der Offset im Konfigurationsraum
 *
 * @return Der Wert an diesem Offset
 * \elseif english
 * Reads a word (16 bit) from the PCI configuration space of a PCI device
 *
 * @param device The device
 * @param offset The offset in the configuration space
 *
 * @return The corresponding value
 * \endif
 */
uint16_t cdi_pci_config_readw(struct cdi_pci_device* device, uint8_t offset)
{
	return (uint16_t)pci_readConfig(device->bus, device->dev, device->function, offset, 2);
}

/**
 * \if german
 * Schreibt ein Word (16 Bit) in den PCI-Konfigurationsraum eines PCI-Geraets
 *
 * @param device Das Geraet
 * @param offset Der Offset im Konfigurationsraum
 * @param value Der zu setzende Wert
 * \elseif english
 * Writes a word (16 bit) into the PCI configuration space of a PCI device
 *
 * @param device The device
 * @param offset The offset in the configuration space
 * @param value Value to be set
 * \endif
 */
void cdi_pci_config_writew(struct cdi_pci_device* device, uint8_t offset, uint16_t value)
{
	pci_writeConfig(device->bus, device->dev, device->function, offset, 2, value);
}
